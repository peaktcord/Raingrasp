"""Run a Python script as a test, under the pypy toolchain.

There is no rules_python in this build. This wraps source-tree checks as tests
that run with the interpreter the decomp toolchain already provides, with the
required sources supplied as data.
"""

load("@rules_decomp//toolchains/pypy:toolchain.bzl", "PYPY_TOOLCHAIN_TYPE")

def _python_check_test_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]
    interpreter = runtime.interpreter

    # The test runs from the runfiles root, which is where the checked sources
    # sit, so the script's own relative paths resolve unchanged.
    launcher = ctx.actions.declare_file(ctx.label.name + ".bat")
    ctx.actions.write(
        output = launcher,
        content = "@echo off\r\n\"%s\" -B \"%s\" %%*\r\n" % (
            interpreter.short_path.replace("/", "\\"),
            ctx.file.script.short_path.replace("/", "\\"),
        ),
        is_executable = True,
    )

    runfiles = ctx.runfiles(
        files = [ctx.file.script] + ctx.files.srcs,
        transitive_files = runtime.files,
    )
    return [DefaultInfo(executable = launcher, runfiles = runfiles)]

python_check_test = rule(
    implementation = _python_check_test_impl,
    test = True,
    attrs = {
        "script": attr.label(allow_single_file = [".py"], mandatory = True),
        "srcs": attr.label_list(allow_files = True),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)
