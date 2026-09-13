"""Intake: JAR identification, .lmp expansion, staleness and tree verification.

Sealed. Every JAR here is synthesised in a temp directory from the *shapes* the
two real archives have, so this runs without the private game files. The shapes
themselves are pinned by `game_jars_inventory_sentinel_test`, which does read
them, so the two tests together cover what one of them cannot.

The cases worth stating outright, because each of them is a real failure this
code exists to prevent:

  - **A Stormhold JAR whose manifest names no game.** The real one's
    `MIDlet-Name` is the series title `The Elder Scrolls`. A cross-check that
    demanded corroboration would refuse every genuine Stormhold JAR, so `None`
    from the manifest has to be an accepted answer rather than a failure.

  - **A JAR whose two signals disagree.** Refusing beats guessing: unpacking a
    misidentified JAR into the wrong variant tree yields a game that boots and
    is quietly wrong.

  - **A tree missing an expanded archive.** `imgfiles.lmp` is read directly by
    `ESGame::createImageFromFile` to build the sprite table, so removing it
    after expansion segfaults startup rather than producing a missing-file
    error. `verify` has to name that, because the crash is a long way from its
    cause.
"""

import io
import json
import os
import shutil
import struct
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bazel"))

import intake  # noqa: E402


FAILURES = []


def check(ok, what):
    if not ok:
        FAILURES.append(what)
        sys.stdout.write("FAIL: %s\n" % what)


def make_lmp(members):
    """Builds an archive in the shipped layout: '-' name '-' off32 len16, then data."""
    dir_size = sum(1 + len(name) + 1 + 6 for name, _ in members)
    header = b""
    data = b""
    offset = dir_size
    for name, body in members:
        header += b"-" + name.encode("ascii") + b"-"
        header += struct.pack(">IH", offset, len(body))
        data += body
        offset += len(body)
    return header + data


DAWNSTAR_MANIFEST = (
    "Manifest-Version: 1.0\r\n"
    "MIDlet-1: DawnStar, icon3650.png, ESGame\r\n"
    "MIDlet-Name: DawnStar by milaro\r\n"
    "MIDlet-Version: 1.0.0\r\n"
)

# The real Stormhold manifest names the series and never the game. This is the
# whole reason the manifest cannot be the deciding signal.
STORMHOLD_MANIFEST = (
    "Manifest-Version: 1.0\r\n"
    "MIDlet-Name: The Elder Scrolls\r\n"
    "MIDlet-1: The Elder Scrolls, icon3650.png, ESGame\r\n"
    "MIDlet-Version: 1.0.10\r\n"
)


def write_jar(path, entries):
    with zipfile.ZipFile(path, "w") as zf:
        for name, body in entries:
            if isinstance(body, str):
                body = body.encode("utf-8")
            zf.writestr(name, body)


DAT_MEMBERS = [
    ("charin.dat", b"CHARIN-DATA"),
    ("helptext.dat", b"HELP"),
    ("geomin.dat", b""),
]
IMG_MEMBERS = [
    ("panel.png", b"PNG-PANEL"),
    ("icons.png", b"PNG-ICONS"),
]


def dawnstar_jar(path, manifest=DAWNSTAR_MANIFEST):
    write_jar(path, [
        ("META-INF/MANIFEST.MF", manifest),
        ("ESGame.class", b"\xca\xfe\xba\xbe"),
        ("datfiles.lmp", make_lmp(DAT_MEMBERS)),
        ("imgfiles.lmp", make_lmp(IMG_MEMBERS)),
        ("splashtop.png", b"PNG-TOP"),
        ("splashbot.png", b"PNG-BOT"),
        ("icon3650.png", b"PNG-ICON"),
    ])


def stormhold_jar(path, manifest=STORMHOLD_MANIFEST):
    write_jar(path, [
        ("META-INF/MANIFEST.MF", manifest),
        ("ESGame.class", b"\xca\xfe\xba\xbe"),
        ("charin.dat", b"CHARIN-DATA"),
        ("itemsin.dat", b"ITEMS"),
        ("baglarge.cus", b"CUS-BAG"),
        ("crystalfar.cus", b"CUS-CRYSTAL"),
        ("splashtop.png", b"PNG-TOP"),
        ("icon3650.png", b"PNG-ICON"),
    ])


# ------------------------------------------------------------------- .lmp


def test_parse_lmp():
    data = make_lmp(DAT_MEMBERS)
    members = intake.parse_lmp(data)
    check(len(members) == 3, "lmp: all members parsed")
    check(members[0] == ("charin.dat", b"CHARIN-DATA"), "lmp: first member")
    check(members[2][1] == b"", "lmp: zero-length member is empty, not missing")
    check([n for n, _ in members] == ["charin.dat", "helptext.dat", "geomin.dat"],
          "lmp: directory order preserved")

    check(intake.parse_lmp(b"") == [], "lmp: empty input yields no members")

    # A member running past the end is damage, not a short read.
    truncated = data[:-4]
    try:
        intake.parse_lmp(truncated)
        check(False, "lmp: overrunning member should be refused")
    except intake.Refused:
        check(True, "lmp: overrunning member refused")

    # A directory entry cut off mid-header.
    try:
        intake.parse_lmp(b"-name-\x00\x00")
        check(False, "lmp: truncated entry should be refused")
    except intake.Refused:
        check(True, "lmp: truncated entry refused")


# --------------------------------------------------------- identification


def test_identify(tmp):
    ds = os.path.join(tmp, "ds.jar")
    sh = os.path.join(tmp, "sh.jar")
    dawnstar_jar(ds)
    stormhold_jar(sh)

    variant, evidence = intake.identify(ds)
    check(variant == "dawnstar", "identify: dawnstar by members")
    check(evidence["manifest_corroborates"], "identify: dawnstar manifest agrees")

    variant, evidence = intake.identify(sh)
    check(variant == "stormhold", "identify: stormhold by members")
    # The case that would break a stricter cross-check.
    check(evidence["by_manifest"] is None,
          "identify: stormhold manifest names no game, and that is accepted")

    # Filenames must not matter: the same bytes under a misleading name.
    misnamed = os.path.join(tmp, "stormhold-totally.jar")
    dawnstar_jar(misnamed)
    variant, _ = intake.identify(misnamed)
    check(variant == "dawnstar", "identify: content wins over a misleading filename")

    # Signals that disagree are refused rather than resolved.
    conflicted = os.path.join(tmp, "conflicted.jar")
    dawnstar_jar(conflicted, manifest="MIDlet-Name: Stormhold\r\n")
    try:
        intake.identify(conflicted)
        check(False, "identify: conflicting signals should be refused")
    except intake.Refused as exc:
        check("manifest" in str(exc), "identify: conflict refused, and says why")

    # Something that is neither game.
    junk = os.path.join(tmp, "junk.jar")
    write_jar(junk, [("readme.txt", "not a game")])
    try:
        intake.identify(junk)
        check(False, "identify: unrecognised jar should be refused")
    except intake.Refused:
        check(True, "identify: unrecognised jar refused")


# ----------------------------------------------------------------- unpack


def test_unpack(tmp):
    ds = os.path.join(tmp, "ds.jar")
    dawnstar_jar(ds)
    out = os.path.join(tmp, "tree-ds")
    variant, written = intake.unpack(ds, out)
    check(variant == "dawnstar", "unpack: identified while unpacking")

    # Members are expanded so override/ can shadow them per file.
    for name, body in DAT_MEMBERS + IMG_MEMBERS:
        path = os.path.join(out, name)
        check(os.path.isfile(path), "unpack: %s expanded" % name)
        if os.path.isfile(path):
            with open(path, "rb") as handle:
                check(handle.read() == body, "unpack: %s bytes match" % name)

    # ...and the archives are kept, because the game reads imgfiles.lmp itself.
    check(os.path.isfile(os.path.join(out, "datfiles.lmp")), "unpack: datfiles.lmp kept")
    check(os.path.isfile(os.path.join(out, "imgfiles.lmp")), "unpack: imgfiles.lmp kept")

    stamp = intake.read_stamp(out)
    check(stamp is not None, "unpack: stamp written")
    check(stamp["variant"] == "dawnstar", "stamp: records the variant")
    check(stamp["source"]["size"] == os.path.getsize(ds), "stamp: records jar size")
    check(len(stamp["source"]["sha256"]) == 64, "stamp: records jar hash")
    expanded = dict((e["archive"], e["members"]) for e in stamp["archives_expanded"])
    check(expanded.get("datfiles.lmp") == 3, "stamp: datfiles member count")
    check(expanded.get("imgfiles.lmp") == 2, "stamp: imgfiles member count")

    # Stormhold ships loose, so nothing is expanded and that is not a problem.
    sh = os.path.join(tmp, "sh.jar")
    stormhold_jar(sh)
    out_sh = os.path.join(tmp, "tree-sh")
    variant, _ = intake.unpack(sh, out_sh)
    check(variant == "stormhold", "unpack: stormhold identified")
    sh_stamp = intake.read_stamp(out_sh)
    check(sh_stamp["archives_expanded"] == [], "unpack: nothing to expand for stormhold")
    check(os.path.isfile(os.path.join(out_sh, "baglarge.cus")), "unpack: .cus written")

    # A JAR dropped on the wrong picker row is reported, not unpacked.
    wrong = os.path.join(tmp, "tree-wrong")
    try:
        intake.unpack(ds, wrong, expect="stormhold")
        check(False, "unpack: mismatched --expect should be refused")
    except intake.Refused as exc:
        check("dawnstar" in str(exc), "unpack: mismatch refused and names the real game")
    check(not os.path.isfile(os.path.join(wrong, "stamp.json")),
          "unpack: a refused intake writes no stamp")


def test_traversal(tmp):
    """A zip entry escaping the output directory is an error, not something to
    sanitise quietly: these archives are hash-pinned, so a traversal entry means
    the manifest is wrong."""
    evil = os.path.join(tmp, "evil.jar")
    with zipfile.ZipFile(evil, "w") as zf:
        zf.writestr("META-INF/MANIFEST.MF", DAWNSTAR_MANIFEST)
        zf.writestr("datfiles.lmp", make_lmp(DAT_MEMBERS))
        zf.writestr("imgfiles.lmp", make_lmp(IMG_MEMBERS))
        zf.writestr("../escaped.txt", b"nope")
    try:
        intake.unpack(evil, os.path.join(tmp, "tree-evil"))
        check(False, "unpack: traversal entry should be refused")
    except intake.Refused as exc:
        check("outside" in str(exc), "unpack: traversal refused")


# ------------------------------------------------------ staleness, verify


def test_stale_and_verify(tmp):
    ds = os.path.join(tmp, "ds.jar")
    dawnstar_jar(ds)
    out = os.path.join(tmp, "tree-stale")
    intake.unpack(ds, out)

    check(intake.is_stale(out, ds) is None, "stale: a fresh tree is current")
    check(intake.verify(out) == [], "verify: a fresh tree is intact")

    # A different JAR with the same shape is a swap, and must be noticed by hash
    # rather than by shape.
    other = os.path.join(tmp, "other.jar")
    dawnstar_jar(other, manifest=DAWNSTAR_MANIFEST + "X-Note: different\r\n")
    reason = intake.is_stale(out, other)
    check(reason is not None and "different JAR" in reason,
          "stale: a swapped jar is detected by hash")

    # An unstamped directory is not an intake tree at all.
    bare = os.path.join(tmp, "bare")
    os.makedirs(bare)
    check(intake.is_stale(bare) is not None, "stale: unstamped tree reported")
    check(intake.verify(bare) != [], "verify: unstamped tree reported")

    # An older layout needs re-unpacking even when the JAR is unchanged.
    aged = os.path.join(tmp, "tree-aged")
    shutil.copytree(out, aged)
    stamp = intake.read_stamp(aged)
    stamp["unpacker_version"] = intake.UNPACKER_VERSION - 1
    with io.open(os.path.join(aged, "stamp.json"), "w", encoding="utf-8") as handle:
        handle.write(json.dumps(stamp))
    reason = intake.is_stale(aged, ds)
    check(reason is not None and "older intake" in reason,
          "stale: an older unpacker version is detected")

    # The case that segfaults rather than erroring: an expanded archive removed.
    maimed = os.path.join(tmp, "tree-maimed")
    shutil.copytree(out, maimed)
    os.remove(os.path.join(maimed, "imgfiles.lmp"))
    problems = intake.verify(maimed)
    check(len(problems) == 1 and "imgfiles.lmp" in problems[0],
          "verify: a removed archive is reported as damage")
    check("crash" in problems[0],
          "verify: and the message names the consequence, not just the absence")


def main():
    tmp = tempfile.mkdtemp(prefix="intake-test-")
    try:
        test_parse_lmp()
        test_identify(tmp)
        test_unpack(tmp)
        test_traversal(tmp)
        test_stale_and_verify(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if FAILURES:
        sys.stdout.write("%d check(s) failed\n" % len(FAILURES))
        return 1
    sys.stdout.write("intake: all checks passed\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
