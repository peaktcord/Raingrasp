"""Identify a game JAR by content and unpack it into a variant tree.

This is Phase 3's intake. It answers two questions the runtime must not have to
ask: *which game is this*, and *where do its bytes live*.

**Identification is by content, never by filename.** A user's download is called
whatever their browser named it, so the member list is the only reliable signal:

    Dawnstar    packs its data into datfiles.lmp and imgfiles.lmp
    Stormhold   ships its .dat and .cus files loose

`META-INF/MANIFEST.MF` corroborates but cannot decide alone, and the reason is
worth stating: Dawnstar's `MIDlet-1` says `DawnStar`, but Stormhold's
`MIDlet-Name` is the series title `The Elder Scrolls` and names the game
nowhere. So members decide, the manifest cross-checks, and a disagreement is
refused rather than guessed -- unpacking a misidentified JAR into the wrong
variant tree would produce a game that boots and is subtly wrong, which is far
worse than a refusal.

**`.lmp` archives are expanded alongside, never instead of, themselves.** Once
`datfiles.lmp`'s members are loose in the tree, the archive stops being special
for *data*: the members are ordinary files in the variant layer, so
`override/helptext.dat` shadows one without anybody repacking an archive.

The archives are nonetheless always kept, and `imgfiles.lmp` is why. Expanding
it is useful -- it makes `override/panel.png` possible the same way -- but it
does **not** make the archive redundant, because `Game::createImageFromFile`
reads `imgfiles.lmp` itself to build an image table keyed by byte offset, and
`Game::createImage` serves every sprite out of that table rather than through
the resource path. Delete the archive and the table is empty, `createImage`
returns null, and the game dereferences it during startup.

So: expand for overridability, keep for correctness. A tree missing its `.lmp`
files is not a supported state, and `verify` reports it as damage rather than as
a tree that merely lacks a fallback.

**A stamp records what was unpacked.** `stamp.json` holds the source JAR's size
and hash plus this unpacker's version, which is what makes "did the user swap
JARs, is this tree stale" answerable without rescanning every file.

Usage:
    intake.py identify <archive.jar>
    intake.py unpack <archive.jar> <output-dir> [--expect <variant>]
    intake.py verify <tree-dir>
"""

import hashlib
import io
import json
import os
import struct
import sys
import zipfile

# Bumped when the unpacked layout changes in a way that makes an existing tree
# wrong rather than merely old. A tree whose stamp records a lower version needs
# re-unpacking; one that merely records a different JAR hash needs the user's
# attention instead.
UNPACKER_VERSION = 1

VARIANTS = ("dawnstar", "stormhold")


class Refused(Exception):
    """Identification failed, or the two signals disagreed."""


# --------------------------------------------------------------- the .lmp
#
# Layout, confirmed against the shipped archive rather than from the prose:
# a run of `'-' name '-' offset:be32 size:be16` directory entries packed at the
# start of the file, terminated by the first byte that is not `-`, then the data
# region. See docs/DATA_FORMATS.md, which described a different (and wrong)
# format until Phase 3.


def parse_lmp(data):
    """Returns [(name, bytes)] in directory order. Raises on a damaged archive."""
    members = []
    pos = 0
    while pos < len(data) and data[pos:pos + 1] == b"-":
        pos += 1
        end = data.find(b"-", pos)
        if end < 0:
            raise Refused("lmp: end of archive while reading a member name")
        name = data[pos:end].decode("ascii", "replace")
        pos = end + 1
        if pos + 6 > len(data):
            raise Refused("lmp: truncated directory entry for %s" % name)
        offset, size = struct.unpack(">IH", data[pos:pos + 6])
        pos += 6
        if offset + size > len(data):
            raise Refused(
                "lmp: member %s extends past the end of the archive" % name)
        members.append((name, data[offset:offset + size]))
    return members


# -------------------------------------------------------- identification


def _members(zf):
    return set(
        info.filename for info in zf.infolist() if not info.is_dir())


def _manifest(zf):
    for name in ("META-INF/MANIFEST.MF", "META-INF/manifest.mf"):
        try:
            return zf.read(name).decode("utf-8", "replace")
        except KeyError:
            continue
    return ""


def _by_members(names):
    lowered = set(n.lower() for n in names)
    has_lmp = "datfiles.lmp" in lowered and "imgfiles.lmp" in lowered
    # Stormhold's tell is loose data plus the .cus sprites Dawnstar has none of.
    loose_dat = any(n.endswith(".dat") for n in lowered)
    has_cus = any(n.endswith(".cus") for n in lowered)
    if has_lmp and not has_cus:
        return "dawnstar"
    if has_cus and loose_dat and not has_lmp:
        return "stormhold"
    return None


def _by_manifest(text):
    """The cross-check. Returns a variant, or None when the manifest is silent.

    Deliberately weaker than the member check. Stormhold's manifest names the
    series and not the game, so `None` here is the *expected* answer for a valid
    Stormhold JAR and must never be treated as a failure.
    """
    lowered = text.lower()
    if "dawnstar" in lowered:
        return "dawnstar"
    if "stormhold" in lowered:
        return "stormhold"
    return None


def identify(path):
    """Returns (variant, evidence). Raises Refused when it cannot be sure."""
    with zipfile.ZipFile(path) as zf:
        names = _members(zf)
        manifest = _manifest(zf)

    by_members = _by_members(names)
    by_manifest = _by_manifest(manifest)

    if by_members is None:
        raise Refused(
            "cannot identify %s: its members match neither game. Dawnstar packs "
            "datfiles.lmp and imgfiles.lmp; Stormhold ships .dat and .cus files "
            "loose." % os.path.basename(path))

    # A manifest that names the *other* game is the case worth refusing loudly:
    # it means one of the two signals is wrong, and guessing which would risk
    # unpacking into the wrong tree.
    if by_manifest is not None and by_manifest != by_members:
        raise Refused(
            "refusing %s: its members look like %s but its manifest says %s. "
            "One of the two is wrong, and guessing could unpack it into the "
            "wrong game's tree." % (os.path.basename(path), by_members, by_manifest))

    evidence = {
        "by_members": by_members,
        "by_manifest": by_manifest,
        "manifest_corroborates": by_manifest == by_members,
        "member_count": len(names),
    }
    return by_members, evidence


# --------------------------------------------------------------- unpacking


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            chunk = handle.read(1 << 16)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def unpack(archive, out_dir, expect=None):
    """Unpacks `archive` into `out_dir`, expanding any .lmp it contains.

    Returns (variant, files_written). Raises Refused when identification fails
    or contradicts `expect` -- which is how a JAR dropped onto the wrong picker
    row is reported rather than unpacked.
    """
    variant, evidence = identify(archive)
    if expect is not None and expect != variant:
        raise Refused(
            "that is a %s JAR, not %s. Drop it on the %s row, or let intake "
            "unpack it into %s/ for the other binary."
            % (variant, expect, variant, variant))

    root = os.path.abspath(out_dir)
    if not os.path.isdir(root):
        os.makedirs(root)

    written = []
    archives_expanded = []

    def write(name, data):
        # Path traversal is an error rather than something to sanitise away:
        # these archives are hash-pinned, so a traversal entry would mean the
        # manifest is wrong, and that is worth failing loudly over.
        target = os.path.abspath(os.path.join(root, name))
        if not target.startswith(root + os.sep) and target != root:
            raise Refused("refusing entry outside the output directory: %s" % name)
        parent = os.path.dirname(target)
        if parent and not os.path.isdir(parent):
            os.makedirs(parent)
        with open(target, "wb") as dst:
            dst.write(data)
        written.append(name)

    content = hashlib.sha256()
    with zipfile.ZipFile(archive) as zf:
        # Name order, so two archives holding the same files agree however their
        # central directories happened to be laid out. The framing below --
        # name, NUL, big-endian length, member digest -- is shared with
        # `content_tree_digest` in bazel/jar_inventory.py and with
        # `contentDigest` in the native unpacker; all three must stay in step.
        for info in sorted(
            (e for e in zf.infolist() if not e.is_dir()),
            key=lambda e: e.filename,
        ):
            content.update(info.filename.encode("utf-8"))
            content.update(b"\0")
            content.update(len(zf.read(info)).to_bytes(8, byteorder="big"))
            content.update(hashlib.sha256(zf.read(info)).digest())

        for info in zf.infolist():
            if info.is_dir():
                continue
            data = zf.read(info)
            write(info.filename, data)
            # Expand the archive *as well as* keeping it. Keeping it costs a few
            # kilobytes and means a tree stays readable by the runtime's archive
            # layer; expanding it is what makes override/ work per member.
            if info.filename.lower().endswith(".lmp"):
                members = parse_lmp(data)
                for name, body in members:
                    write(name, body)
                archives_expanded.append(
                    {"archive": info.filename, "members": len(members)})

    if not written:
        raise Refused("%s contained no files" % archive)

    stamp = {
        "unpacker_version": UNPACKER_VERSION,
        "variant": variant,
        # Keyed on content, not on the archive: the two Stormhold JARs hold the
        # same files in different ZIP packaging, so the archive hash above would
        # call them different games' data and re-unpack on a swap. The native
        # unpacker writes this same field, computed the same way, so a tree from
        # either is legible to both.
        "content_sha256": content.hexdigest(),
        "source": {
            "name": os.path.basename(archive),
            "size": os.path.getsize(archive),
            "sha256": _sha256(archive),
        },
        "identification": evidence,
        "files_written": len(written),
        "archives_expanded": archives_expanded,
    }
    with io.open(os.path.join(root, "stamp.json"), "w", encoding="utf-8") as handle:
        handle.write(json.dumps(stamp, indent=2, sort_keys=True))
        handle.write("\n")

    return variant, written


def read_stamp(tree):
    """Returns the stamp dict for an unpacked tree, or None when absent."""
    path = os.path.join(tree, "stamp.json")
    if not os.path.isfile(path):
        return None
    try:
        with io.open(path, encoding="utf-8") as handle:
            return json.load(handle)
    except ValueError:
        return None


def is_stale(tree, archive=None):
    """Why `tree` needs re-unpacking, or None when it is current.

    This is the question the stamp exists to answer without rescanning: an
    unpacked tree from an older layout, or one produced from a different JAR
    than the user now has, is stale in two quite different ways and the caller
    wants to tell them apart.
    """
    stamp = read_stamp(tree)
    if stamp is None:
        return "no stamp.json: the tree was not produced by intake"
    if stamp.get("unpacker_version", 0) < UNPACKER_VERSION:
        return "unpacked by an older intake (version %s, current %s)" % (
            stamp.get("unpacker_version"), UNPACKER_VERSION)
    if archive is not None:
        want = _sha256(archive)
        got = stamp.get("source", {}).get("sha256")
        if want != got:
            return "unpacked from a different JAR (tree %s, supplied %s)" % (
                (got or "?")[:12], want[:12])
    return None


def verify(tree):
    """Returns a list of problems with an unpacked tree; empty means intact.

    Distinct from `is_stale`, which asks whether the tree matches the JAR the
    user now has. This asks whether the tree is *usable at all*, and the case it
    exists for is a missing `.lmp`: `Game::createImageFromFile` reads
    `imgfiles.lmp` directly to build its sprite table, so a Dawnstar tree
    without it segfaults during startup rather than failing to find a file.
    That is a hard crash a long way from its cause, so it is worth naming here.
    """
    stamp = read_stamp(tree)
    if stamp is None:
        return ["no stamp.json: the tree was not produced by intake"]

    problems = []
    for entry in stamp.get("archives_expanded", []):
        name = entry.get("archive")
        if name and not os.path.isfile(os.path.join(tree, name)):
            problems.append(
                "%s is missing. Its members were expanded into the tree, but the "
                "archive itself is still required: the game reads it directly to "
                "build its image table, and without it startup crashes." % name)

    if not stamp.get("files_written"):
        problems.append("stamp.json records no files_written")

    return problems


# ------------------------------------------------------------------- cli


def main(argv):
    if len(argv) >= 3 and argv[1] == "identify":
        try:
            variant, evidence = identify(argv[2])
        except Refused as exc:
            sys.stderr.write("%s\n" % exc)
            return 1
        sys.stdout.write("%s\n" % variant)
        sys.stdout.write("  members say:  %s\n" % evidence["by_members"])
        sys.stdout.write("  manifest says: %s\n" % (
            evidence["by_manifest"] or "nothing (expected for Stormhold)"))
        return 0

    if len(argv) >= 4 and argv[1] == "unpack":
        expect = None
        if "--expect" in argv:
            expect = argv[argv.index("--expect") + 1]
        try:
            variant, written = unpack(argv[2], argv[3], expect)
        except Refused as exc:
            sys.stderr.write("%s\n" % exc)
            return 1
        sys.stdout.write("unpacked %s: %d files into %s\n"
                         % (variant, len(written), argv[3]))
        return 0

    if len(argv) >= 3 and argv[1] == "verify":
        problems = verify(argv[2])
        if problems:
            for line in problems:
                sys.stderr.write("%s\n" % line)
            return 1
        sys.stdout.write("%s: intact\n" % argv[2])
        return 0

    sys.stderr.write(
        "usage: intake.py identify <archive.jar>\n"
        "       intake.py unpack <archive.jar> <output-dir> [--expect <variant>]\n"
        "       intake.py verify <tree-dir>\n")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
