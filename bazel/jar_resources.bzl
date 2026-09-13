"""Unpack a game JAR into a directory the converted code can load from.

The games read their data at runtime the way the MIDlet did on a handset --
`Resources::setRoot` points at a directory and `getResourceAsStream` opens files
under it. Until now that directory was produced by someone running `unzip` once
by hand, and every golden test globbed whatever was sitting there.

That left a gap in the provenance. `verified_files` hash-pins the JARs, but the
verification stopped at the JAR: the unpacked resources the tests actually read
were unchecked, so a corrupted `datfiles.lmp` would have been picked up silently
and baked into any baseline regenerated afterwards. A stable baseline that is
wrong is worse than no baseline, and it gets more dangerous as more of them
accumulate.

This closes it. The output is a tree artifact, so there is no need to enumerate
the JAR's entries, and it is derived from the verified JAR, so provenance runs
unbroken from the pinned hash to every golden file.

Since Phase 3 the unpacking is `bazel/intake.py`, the same tool a user's own JAR
goes through -- so the tree the golden tests read is produced by exactly the code
path a player's install uses, rather than by a second implementation that could
drift from it. That also means `.lmp` members are expanded here, and the tests
therefore exercise the layer stack finding them loose.
"""

load("@rules_decomp//decomp:verified_inputs.bzl", "VerifiedFilesInfo")
load("@rules_decomp//toolchains/pypy:toolchain.bzl", "PYPY_TOOLCHAIN_TYPE")

def _jar_resources_impl(ctx):
    runtime = ctx.toolchains[PYPY_TOOLCHAIN_TYPE]
    verified = ctx.attr.jar[VerifiedFilesInfo]
    jars = verified.files.to_list()

    source = None
    for candidate in jars:
        if candidate.basename == ctx.attr.jar_name:
            source = candidate
    if source == None:
        fail("%s not among the verified jars: %s" % (
            ctx.attr.jar_name,
            [f.basename for f in jars],
        ))

    output = ctx.actions.declare_directory(ctx.label.name)

    # The verification marker is an input purely so the unpack cannot run before
    # the hash check has passed.
    args = ctx.actions.args()
    args.add("-B")
    args.add(ctx.file._unpacker.path)
    args.add("unpack")
    args.add(source.path)
    args.add(output.path)
    # Identification is by content, so the expected variant is an assertion
    # rather than a hint: if the JAR under this name is not the game this target
    # claims, the build fails here instead of unpacking it into the wrong tree.
    args.add("--expect")
    args.add(ctx.attr.variant)

    ctx.actions.run(
        executable = runtime.interpreter,
        arguments = [args],
        inputs = depset(
            direct = [ctx.file._unpacker, source, verified.verification],
            transitive = [runtime.files],
        ),
        outputs = [output],
        mnemonic = "UnpackGameJar",
        progress_message = "Unpacking %s" % ctx.attr.jar_name,
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

jar_resources = rule(
    implementation = _jar_resources_impl,
    doc = "Unpacks one verified JAR into a tree artifact of game resources.",
    attrs = {
        "jar": attr.label(
            providers = [VerifiedFilesInfo],
            mandatory = True,
            doc = "A verified_files target holding the game JARs.",
        ),
        "jar_name": attr.string(
            mandatory = True,
            doc = "Basename of the JAR to unpack, e.g. tes-travels-dawnstar-1.0.0.jar.",
        ),
        "variant": attr.string(
            mandatory = True,
            values = ["dawnstar", "stormhold"],
            doc = "Which game this JAR must identify as, checked by content.",
        ),
        "_unpacker": attr.label(
            default = Label("//bazel:intake.py"),
            allow_single_file = [".py"],
        ),
    },
    toolchains = [PYPY_TOOLCHAIN_TYPE],
)
