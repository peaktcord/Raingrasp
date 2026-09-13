"""The two unpackers must produce the same tree.

`bazel/intake.py` unpacks a JAR during the build; `platform/intake.cpp` unpacks
one when a player drops it on the .exe. They implement the same rules, and
either one's tree is read by the same runtime, so a divergence between them is a
game that behaves differently depending on how its data got there -- the worst
kind of bug to chase, because nothing about the running game points at intake.

This diffs the two trees file by file. `stamp.json` is excluded: it deliberately
records which unpacker ran.

It has already earned its place. The first native implementation lowercased
`.lmp` member names while the Python one preserved them, which on Windows is
invisible -- every file opens either way. On a case-sensitive filesystem, and
therefore under wasm's MEMFS, three images would simply have vanished:
`Game::createImageFromFile` keys its sprite table by the names inside
`imgfiles.lmp` and `Game::createImage` looks them up case-sensitively, asking
for `floorIce.png`, `mformaLogo.png` and `vir2lLogo.png` exactly so.

Usage: intake_parity_test.py <native_dump.exe> <intake.py> <jar> [<jar> ...]
"""

import filecmp
import os
import shutil
import subprocess
import sys
import tempfile


def tree_files(root):
    out = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            if name == "stamp.json":
                continue
            full = os.path.join(dirpath, name)
            out.append(os.path.relpath(full, root).replace(os.sep, "/"))
    return sorted(out)


def main(argv):
    if len(argv) < 4:
        sys.stderr.write(
            "usage: intake_parity_test.py <native_dump.exe> <intake.py> <jar> [...]\n")
        return 2

    native = os.path.abspath(argv[1])
    script = os.path.abspath(argv[2])
    jars = [os.path.abspath(j) for j in argv[3:]]

    failures = 0
    tmp = tempfile.mkdtemp(prefix="intake-parity-")
    try:
        for index, jar in enumerate(jars):
            py_tree = os.path.join(tmp, "py%d" % index)
            cpp_tree = os.path.join(tmp, "cpp%d" % index)

            subprocess.run([sys.executable, "-B", script, "unpack", jar, py_tree],
                           check=True, stdout=subprocess.DEVNULL)
            subprocess.run([native, "unpack", jar, cpp_tree],
                           check=True, stdout=subprocess.DEVNULL)

            py_files = tree_files(py_tree)
            cpp_files = tree_files(cpp_tree)
            name = os.path.basename(jar)

            if py_files != cpp_files:
                failures += 1
                sys.stdout.write("%s: the two unpackers wrote different files\n" % name)
                for missing in sorted(set(py_files) - set(cpp_files)):
                    sys.stdout.write("  only from intake.py:   %s\n" % missing)
                for extra in sorted(set(cpp_files) - set(py_files)):
                    sys.stdout.write("  only from native:      %s\n" % extra)
                continue

            differing = []
            for rel in py_files:
                a = os.path.join(py_tree, rel.replace("/", os.sep))
                b = os.path.join(cpp_tree, rel.replace("/", os.sep))
                if not filecmp.cmp(a, b, shallow=False):
                    differing.append(rel)

            if differing:
                failures += 1
                sys.stdout.write("%s: %d file(s) differ in content\n"
                                 % (name, len(differing)))
                for rel in differing[:10]:
                    sys.stdout.write("  %s\n" % rel)
            else:
                sys.stdout.write("%s: %d files identical from both unpackers\n"
                                 % (name, len(py_files)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if failures:
        sys.stdout.write("%d archive(s) unpacked differently\n" % failures)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
