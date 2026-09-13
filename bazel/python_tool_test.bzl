"""Run a Python test that drives a built binary over private inputs.

`python_check_test` runs a script over the source tree; this runs one that needs
an executable and data files as well, with their runfiles paths substituted into
the arguments. The parity test is the case it exists for: it has to run *both*
unpackers, one of which is a compiled binary and the other a script, over the
user's own JARs.
"""

load("@rules_decomp//toolchains/pypy:toolchain.bzl", "PYPY_TOOLCHAIN_TYPE")

def _python_tool_test_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]

    parts = [ctx.file.script.short_path, ctx.executable.tool.short_path]
    parts += [f.short_path for f in ctx.files.args_files]
    quoted = " ".join(["\"%s\"" % p.replace("/", "\\") for p in parts])

    launcher = ctx.actions.declare_file(ctx.label.name + ".bat")
    ctx.actions.write(
        output = launcher,
        content = "@echo off\r\n\"%s\" -B %s\r\n" % (
            runtime.interpreter.short_path.replace("/", "\\"),
            quoted,
        ),
        is_executable = True,
    )

    runfiles = ctx.runfiles(
        files = [ctx.file.script, ctx.executable.tool] + ctx.files.args_files,
        transitive_files = runtime.files,
    )
    runfiles = runfiles.merge(ctx.attr.tool[DefaultInfo].default_runfiles)
    return [DefaultInfo(executable = launcher, runfiles = runfiles)]

python_tool_test = rule(
    implementation = _python_tool_test_impl,
    test = True,
    attrs = {
        "script": attr.label(allow_single_file = [".py"], mandatory = True),
        "tool": attr.label(executable = True, cfg = "target", mandatory = True),
        "args_files": attr.label_list(
            allow_files = True,
            doc = "Passed to the script after the tool, as runfiles paths.",
        ),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)
