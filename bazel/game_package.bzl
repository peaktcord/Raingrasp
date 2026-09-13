"""Assemble a runnable game folder in bazel-bin.

`bazel build //:dawnstar_sdl` leaves you with an executable that only runs from
its own runfiles tree, next to a MANIFEST and a `_main` directory that mean
nothing to a player. Handing someone that is not handing them the game, so the
folder people actually got sent was assembled by hand -- which is how
CONTROLS.txt came to exist in `dist/` and nowhere in version control at all.

This rule builds that folder instead. The output is a tree artifact holding the
executable, the DLLs it needs beside it, and the documentation, flat, with
nothing a player has to know about Bazel to interpret.

Two properties are the point:

- **Relocatable.** Everything the package needs is inside it, so it can be
  copied anywhere. The launcher reaches nowhere outside its own directory,
  unlike the repo-relative one in `dist/`, which resolved a JAR through
  `%~dp0` and two levels up and so only worked from inside a checkout.
- **Complete by construction.** The DLLs come from the binary's own runfiles
  rather than from a list kept in parallel with the build, so a new runtime
  dependency is packaged because it is a dependency, not because someone
  remembered. `dist/` was assembled by copying, which is exactly the process
  that loses a file quietly.

No game data is included: the games ship none, and the JAR is the user's own.
Intake finds it at runtime -- see the README.
"""

load("@rules_decomp//toolchains/pypy:toolchain.bzl", "PYPY_TOOLCHAIN_TYPE")

def _game_package_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]

    # The folder name is what a player sees after unzipping, so it is set
    # separately from the target name: Bazel targets are conventionally
    # lowercase and suffixed, and "raingrasp_dist/" is a build system's name
    # for the thing rather than the product's.
    output = ctx.actions.declare_directory(ctx.attr.package_dir or ctx.label.name)

    binary = ctx.executable.binary

    # Take the runtime files from the binary's own runfiles. Anything it needs
    # beside it to start is there by definition, so this cannot drift from what
    # the binary actually loads the way a hand-kept file list does.
    runfiles = ctx.attr.binary[DefaultInfo].default_runfiles.files.to_list()
    payload = [f for f in runfiles if f.extension in ctx.attr.runtime_extensions]

    inputs = [binary] + payload + ctx.files.docs + [ctx.file._packager]

    args = ctx.actions.args()
    args.add("-B")
    args.add(ctx.file._packager.path)
    args.add(output.path)
    args.add(binary.path)
    args.add_all([f.path for f in payload])
    args.add("--")
    args.add_all([f.path for f in ctx.files.docs])

    ctx.actions.run(
        executable = runtime.interpreter,
        arguments = [args],
        inputs = depset(direct = inputs, transitive = [runtime.files]),
        outputs = [output],
        mnemonic = "PackageGame",
        progress_message = "Packaging %s" % ctx.label.name,
        env = {
            "PYTHONDONTWRITEBYTECODE": "1",
            "PYTHONHASHSEED": "0",
        },
    )
    return [DefaultInfo(files = depset([output]))]

game_package = rule(
    implementation = _game_package_impl,
    doc = "Collects a game binary, its runtime DLLs and its docs into one folder.",
    attrs = {
        "binary": attr.label(
            executable = True,
            cfg = "target",
            mandatory = True,
            doc = "The cc_binary to package.",
        ),
        "docs": attr.label_list(
            allow_files = True,
            doc = "Documentation copied in beside the executable.",
        ),
        "package_dir": attr.string(
            doc = "Name of the assembled folder. Defaults to the target name.",
        ),
        "runtime_extensions": attr.string_list(
            default = ["dll"],
            doc = "Runfile extensions taken to be needed beside the executable.",
        ),
        "_packager": attr.label(
            default = Label("//bazel:package_game.py"),
            allow_single_file = [".py"],
        ),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)
