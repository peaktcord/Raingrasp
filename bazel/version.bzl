"""Generates the translation unit holding the build's commit id.

Split out of the platform package so that only this leaf and the binaries that
link it are invalidated when HEAD moves.  A genrule cannot read the status
files, so this is a rule: ctx.version_file is the volatile status, and
ctx.info_file the stable one that workspace_status.bat writes into.
"""

load("@rules_decomp//toolchains/pypy:toolchain.bzl", "PYPY_TOOLCHAIN_TYPE")

def _commit_source_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]
    out = ctx.actions.declare_file(ctx.label.name + ".cpp")

    args = ctx.actions.args()
    args.add("-B")
    args.add(ctx.file._writer.path)
    args.add(ctx.info_file.path)
    args.add(out.path)

    # ctx.info_file is the stable status file, read at action time: its content
    # is not available to Starlark here, so the extraction happens inside the
    # action.  Depending on it is what ties this target to --stamp; a build
    # without stamping still gets a file, just without the key.
    ctx.actions.run(
        executable = runtime.interpreter,
        arguments = [args],
        inputs = depset(
            direct = [ctx.file._writer, ctx.info_file],
            transitive = [runtime.files],
        ),
        outputs = [out],
        mnemonic = "RaingraspVersion",
        progress_message = "Generating %s" % out.short_path,
        env = {
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONHASHSEED": "0",
        },
    )
    return [DefaultInfo(files = depset([out]))]

commit_source = rule(
    implementation = _commit_source_impl,
    attrs = {
        "_writer": attr.label(
            allow_single_file = [".py"],
            default = Label("//bazel:write_version.py"),
        ),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)
