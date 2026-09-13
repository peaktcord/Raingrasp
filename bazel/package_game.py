"""Copy a game binary, its runtime libraries and its docs into one flat folder.

Usage: package_game.py <outdir> <binary> [<runtime>...] -- [<doc>...]

Deliberately flat: a player opens the folder and sees the executable, the
launcher, and the controls. Bazel's runfiles layout -- MANIFEST, `_main/`,
repository directories -- is a build system's business, not theirs.
"""

import os
import shutil
import sys


def main(argv):
    if "--" not in argv:
        sys.stderr.write("usage: package_game.py <outdir> <binary> [rt...] -- [docs...]\n")
        return 2
    split = argv.index("--")
    head, docs = argv[:split], argv[split + 1:]
    if len(head) < 2:
        sys.stderr.write("package_game.py: need an output directory and a binary\n")
        return 2

    outdir, binary, runtimes = head[0], head[1], head[2:]
    os.makedirs(outdir, exist_ok=True)

    for src in [binary] + list(runtimes) + list(docs):
        dest = os.path.join(outdir, os.path.basename(src))
        # A runfiles tree can stage the same DLL under more than one path. The
        # copies are identical, so the first wins and the rest are redundant --
        # but a *differing* second copy means two libraries with one name, which
        # would be ambiguous at load time and is worth failing on rather than
        # resolving by whichever happened to be ordered last.
        if os.path.exists(dest):
            if os.path.getsize(dest) != os.path.getsize(src):
                sys.stderr.write(
                    "package_game.py: two different files named %s\n"
                    % os.path.basename(src))
                return 1
            continue
        # copyfile, not copy2: the mode is deliberately not carried over.
        # Bazel marks tree-artifact contents read-only after the action anyway;
        # the executable bit survives when the folder is copied for use.
        shutil.copyfile(src, dest)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
