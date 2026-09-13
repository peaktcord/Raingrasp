"""Project-local adapters that capture tools run over verified private inputs."""

load("@rules_decomp//decomp:verified_inputs.bzl", "VerifiedFilesInfo")
load("@rules_decomp//toolchains/pypy:toolchain.bzl", "PYPY_TOOLCHAIN_TYPE")

def _private_tool_output_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]
    verified = ctx.attr.input[VerifiedFilesInfo]
    private_files = verified.files.to_list()
    if len(private_files) != 1:
        fail("%s requires exactly one verified private file; got %d" % (
            ctx.label,
            len(private_files),
        ))

    output = ctx.actions.declare_file(ctx.label.name + ".txt")
    args = ctx.actions.args()
    args.add("-B")
    args.add(ctx.file._capture.path)
    args.add("--tool", ctx.executable.tool.path)
    args.add("--input", private_files[0].path)
    args.add("--output", output.path)

    ctx.actions.run(
        executable = runtime.interpreter,
        arguments = [args],
        inputs = depset(
            direct = [
                ctx.file._capture,
                ctx.executable.tool,
                verified.verification,
            ] + private_files,
            transitive = [runtime.files],
        ),
        outputs = [output],
        mnemonic = "PrivateToolOutput",
        progress_message = "Generating private-data output for %s" % ctx.label,
        execution_requirements = {
            "no-remote": "1",
            "no-remote-cache": "1",
            "no-remote-exec": "1",
        },
        env = {
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONHASHSEED": "0",
        },
    )
    return [DefaultInfo(files = depset([output]))]

private_tool_output = rule(
    implementation = _private_tool_output_impl,
    attrs = {
        "input": attr.label(
            mandatory = True,
            providers = [VerifiedFilesInfo],
        ),
        "tool": attr.label(
            cfg = "exec",
            executable = True,
            mandatory = True,
        ),
        "_capture": attr.label(
            allow_single_file = [".py"],
            default = Label("//bazel:capture_tool_output.py"),
        ),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)

def _file_sha256_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]
    output = ctx.actions.declare_file(ctx.label.name + ".sha256")
    args = ctx.actions.args()
    args.add("-B")
    args.add(ctx.file._digest.path)
    args.add("--input", ctx.file.src.path)
    args.add("--output", output.path)

    ctx.actions.run(
        executable = runtime.interpreter,
        arguments = [args],
        inputs = depset(
            direct = [ctx.file._digest, ctx.file.src],
            transitive = [runtime.files],
        ),
        outputs = [output],
        mnemonic = "FileSha256",
        progress_message = "Hashing output for %s" % ctx.label,
        env = {
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONHASHSEED": "0",
        },
    )
    return [DefaultInfo(files = depset([output]))]

file_sha256 = rule(
    implementation = _file_sha256_impl,
    doc = "Writes a lowercase SHA-256 digest for one generated file.",
    attrs = {
        "src": attr.label(allow_single_file = True, mandatory = True),
        "_digest": attr.label(
            allow_single_file = [".py"],
            default = Label("//bazel:file_sha256.py"),
        ),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)

def _resource_tool_output_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]
    resources = ctx.attr.resources[DefaultInfo].files.to_list()
    if len(resources) != 1:
        fail("%s requires exactly one resource tree; got %d" % (
            ctx.label,
            len(resources),
        ))

    output = ctx.actions.declare_file(ctx.label.name + ".tsv")
    args = ctx.actions.args()
    args.add("-B")
    args.add(ctx.file._capture.path)
    args.add("--tool", ctx.executable.tool.path)
    args.add("--input", resources[0].path)
    args.add("--output", output.path)
    for tool_arg in ctx.attr.args:
        args.add("--arg=%s" % tool_arg)
    if ctx.attr.tool_writes_output:
        args.add("--tool-writes-output")

    ctx.actions.run(
        executable = runtime.interpreter,
        arguments = [args],
        inputs = depset(
            direct = [
                ctx.file._capture,
                ctx.executable.tool,
                resources[0],
            ],
            transitive = [
                runtime.files,
                ctx.attr.tool[DefaultInfo].default_runfiles.files,
            ],
        ),
        outputs = [output],
        mnemonic = "ResourceToolOutput",
        progress_message = "Capturing resource-backed output for %s" % ctx.label,
        execution_requirements = {
            "no-remote": "1",
            "no-remote-cache": "1",
            "no-remote-exec": "1",
        },
        env = {
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONHASHSEED": "0",
        },
    )
    return [DefaultInfo(files = depset([output]))]

resource_tool_output = rule(
    implementation = _resource_tool_output_impl,
    doc = "Captures normalized stdout from a tool over one declared resource tree.",
    attrs = {
        "resources": attr.label(mandatory = True),
        "args": attr.string_list(),
        "tool_writes_output": attr.bool(default = False),
        "tool": attr.label(
            cfg = "exec",
            executable = True,
            mandatory = True,
        ),
        "_capture": attr.label(
            allow_single_file = [".py"],
            default = Label("//bazel:capture_tool_output.py"),
        ),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)
