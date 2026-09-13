"""Read the strike table out of a Symbian bitmap font store (.GDR).

Why this exists
---------------
The port draws text with a generic 5x7 bitmap font and measures it with a
monospace approximation (`Font::stringWidth` in `common/ui.hpp` is
`length * charWidth('m')`). Both are documented deviations, but they are
stacked: the metrics decide where `wrapText` breaks lines, so the letterforms
and the line breaks are wrong together.

The real faces ship in `CEUROPE.GDR` with the Series 60 SDK. This reads the
store's header and typeface table so the strikes can be enumerated -- names,
ascent, height, max width, and whether each is proportional -- without needing
the SDK, an emulator, or a device ever again once the output is committed.

Provenance
----------
Series 60 SDK Beta v0.2 (8 November 2001), based on Symbian OS 6.1, file dated
19 October 2001. This is the *C++* SDK: it has no MIDP/CLDC, so it answers what
the glyphs and metrics are, and not which strike `Font.getFont(face, style,
size)` resolves to. That mapping is a separate question -- see docs.

Format
------
Not published in the SDK headers; `fntstore.h` declares the runtime classes
(`TCharacterMetrics`, `CBitmapFont`) but not the on-disk layout. What is below
was derived from the file and then checked against a property the format itself
supplies: every strike whose name ends in a number has a max-height field equal
to that number. CEUROPE agrees 8 of 8, and BROWSEREUR.GDR -- which the parser was
never tuned against -- agrees 6 of 6, so the field order is confirmed by
fourteen independent agreements across two files rather than merely believed.

`main` fails rather than prints when that check does not hold, because a field
order that has silently shifted would otherwise produce a plausible-looking
table of wrong numbers, which is the failure mode worth refusing.

    0x00  u32   UID1 0x10000037   (direct file store)
    0x04  u32   UID2 0x10000039
    0x08  u32   UID3 (0)
    0x0c  u32   checksum / 'S89G'
    0x18  u32   font store UID (per-file: CEUROPE 0x10005979, CALCEUR ...7a,
                BROWSEREUR ...7b -- a file identity, not a format magic)
    0x30  u32   typeface count
    0x34  ...   font bitmap records, each:
                  u32  typeface UID
                  u8   posture        (0 upright, 1 italic)
                  u8   strokeWeight   (0 normal, 1 bold)
                  u8   isProportional (1 = per-glyph widths)
                  u8   maxHeight in pixels   (the NN in LatinBoldNN)
                  u8   maxAscent in pixels
                  u8   maxCharWidth in pixels
                  u8   maxNormalCharWidth in pixels
                  u32  (unused)
                  u32  0x10
                  ...  code section table, 12 bytes per section:
                         u16 firstCode, u16 lastCode,
                         u32 metricsOffset, u32 bitmapOffset
    ...         typeface name table, each entry a length-prefixed UTF-16LE name

At `metricsOffset`: a u32 character count, then 8 bytes per character, which
are `TCharacterMetrics` from `fntstore.h` with its two 16-bit fields packed to
8 bits each -- confirmed by that header's declared field order:

    u16  bitmapOffset   (into the section's bitmap data)
    u8   ascentInPixels
    u8   heightInPixels
    u8   leftAdjustInPixels
    u8   moveInPixels    <- the advance; this is what stringWidth sums
    u8   rightAdjustInPixels
    u8   (padding, always 0)

The `move` column is the whole point: it varies per character (3 for `i`, `I`,
`l` and `.`; 11 for `m`), which is what makes these faces proportional and the
port's `length * charWidth('m')` wrong rather than merely imprecise.

The shipping layout (Nokia 3650 ROM, November 2002)
---------------------------------------------------
The font a real 3650 drew with is not the SDK beta's file, and its metrics are
stored differently. The bitmap record carries three extra words before the
code section table, which therefore sits at +0x1B rather than +0x13:

    +0x0F  u32  offset of this strike's metric table
    +0x13  u32  number of entries in that table
    +0x17  u32  number of code sections

The metric table is a `u32` count followed by 5-byte `TCharacterMetrics`
entries (ascent, height, leftAdjust, move, rightAdjust), shared by every
character in the strike and ordered by how often each shape recurs. A section's
"metrics" block is then only a `u32` count and one `u16` per character: the
character's byte offset into the section's bitmap block, which is itself a
`u32` length followed by the glyph data.

Each glyph's data opens with its metric index, and that byte is where two
sessions of inference failed. It was read out of the device's own reader --
`CFontBitmap::CharacterMetrics` in the ROM's `FntStr.dll`, decompiled with
Ghidra (the SDK's WINS `FNTSTR.DLL` still reads the older 8-byte layout, so it
could not have answered this):

    index = byte0 >> 1
    if byte0 & 1: index += byte1 * 128     (and the glyph data starts one later)

The glyph bits that follow are the row-run stream BITGDI decodes: a 5-bit
header per run -- bit 0 set means the next `count` rows are each stored, clear
means one stored row repeated `count` times -- with `count` in the next 4 bits
and every field packed least-significant-bit first. Row width is
`move - leftAdjust - rightAdjust`.

That decoding is not merely believed: `read_metrics` decodes every glyph and
requires the stream to end inside the glyph's last byte, which a wrong index,
a wrong width or a wrong bit order would each break. All 95 ASCII glyphs of
all five Latin strikes pass, and 469 of the 475 metrics agree with the SDK
beta's, which was a separate file read by a separate code path.

Usage:
  python tools/gdr_dump.py <CEUROPE.GDR> [--write out.tsv]
  python tools/gdr_dump.py <CEUROPE.GDR> --widths <typeface> [--write out.tsv]
  python tools/gdr_dump.py <CEUROPE.GDR> --glyphs <typeface> [--text "..."]
"""

import re
import struct
import sys

# Each font store carries its own UID rather than a shared magic: CEUROPE is
# 0x10005979, CALCEUR 0x1000597a, BROWSEREUR 0x1000597b. What identifies the
# format is the 'S89G' tag at 0x0c and the UID landing in the font-store range.
STORE_TAG = b"S89G"
STORE_UID_PREFIX = 0x100059
TYPEFACE_COUNT_OFFSET = 0x30
FIRST_RECORD_OFFSET = 0x34


NAME_TABLE_SEARCH_START = 0x4D0


def read_name_for(data, uid):
    """The typeface name for `uid`, found by anchoring on the UID itself.

    The name table entries are `<len byte> <2 bytes> <name> <4 bytes> <uid>`,
    but the trailer between name and uid is not fixed across entries, so
    walking the table by stride desynchronises after the first record. The UIDs
    are unambiguous 4-byte values, though, so each name is located by finding
    its UID and stepping *backwards*: for each plausible length, check that the
    descriptor byte at that distance encodes that length (Symbian stores it as
    `len << 2`) and that the characters read as ASCII. Only the true length
    satisfies both.

    Characters are 16-bit but every name here is ASCII, so the low byte of each
    unit is the character.
    """
    # The name table follows every bitmap record, so the UID's *last*
    # occurrence is the one beside its name. Searching forward from a fixed
    # offset found the bitmap record instead in Browsereur.gdr, whose nine
    # records run past that offset, and three of its faces went unnamed.
    position = data.rfind(struct.pack("<I", uid))
    if position < NAME_TABLE_SEARCH_START:
        return None
    for length in range(1, 32):
        text_at = position - 4 - 2 * length
        if text_at - 3 < 0:
            continue
        if data[text_at - 3] >> 2 != length:
            continue
        name = "".join(chr(data[text_at + 2 * k]) for k in range(length))
        if all(32 <= ord(c) < 127 for c in name):
            return name
    return None


def read_strikes(data):
    """The font bitmap records, walked by UID.

    Records are consecutive but not fixed-width -- each carries a code section
    table whose length depends on how many Unicode ranges the strike covers --
    so the walk finds the next record by looking for the next UID rather than
    by adding a stride.
    """
    count = struct.unpack_from("<I", data, TYPEFACE_COUNT_OFFSET)[0]

    # The UIDs are not contiguous -- CEUROPE runs ...10 to ...14 and then jumps
    # to ...1a -- so the walk cannot step by `uid + 1`. Every typeface UID
    # appears exactly twice, once in its bitmap record and once in the name
    # table, so the set is recovered by taking UIDs that occur twice in the
    # store's own UID range, then visiting each bitmap record by its first
    # occurrence.
    candidates = {}
    for offset in range(FIRST_RECORD_OFFSET, len(data) - 4):
        value = struct.unpack_from("<I", data, offset)[0]
        if value >> 8 == STORE_UID_PREFIX:
            candidates.setdefault(value, []).append(offset)
    uids = sorted(uid for uid, at in candidates.items() if len(at) == 2)

    strikes = []
    for uid in uids:
        if len(strikes) == count:
            break
        offset = candidates[uid][0]
        if offset + 11 > len(data):
            continue
        # A UID that occurs twice but names nothing is not a typeface -- the
        # store's own structural UIDs can collide with the "occurs twice" test.
        if read_name_for(data, uid) is None:
            continue
        posture, weight, proportional, height, ascent, maxw, maxnw = data[offset + 4:offset + 11]
        strikes.append({
            "recordOffset": offset,
            "uid": uid,
            "posture": posture,
            "strokeWeight": weight,
            "isProportional": proportional,
            "ascent": ascent,
            "height": height,
            "maxCharWidth": maxw,
            "maxNormalCharWidth": maxnw,
        })
    return strikes


# Where the code section table sits inside a font bitmap record, measured from
# the record's start. The SDK's beta fonts put it at 0x13; the fonts dumped from
# a shipping 3650 carry three extra words first and put it at 0x1b. Rather than
# pick one, `read_sections` searches the plausible range for an offset whose
# first entry looks like a real section -- which is what makes the same parser
# read both builds.
SECTION_TABLE_CANDIDATES = (0x13, 0x1B, 0x17, 0x1F)
SECTION_ENTRY_SIZE = 12
METRIC_SIZE = 8

# The shipping layout: the section table at +0x1B is preceded by the strike's
# metric table pointer and count at +0x0F and +0x13, and each table entry is
# the five TCharacterMetrics bytes with nothing else.
SHIPPING_SECTION_TABLE_OFFSET = 0x1B
SHIPPING_METRIC_TABLE_FIELD = 0x0F
SHIPPING_METRIC_SIZE = 5


def read_sections(data, strike):
    """The code sections a strike covers.

    Each is a contiguous Unicode range with its own metrics and bitmap blocks.
    The table has no count field, so it is walked until an entry stops looking
    like one: a real section is ordered (first <= last), and both its blocks
    land inside the file with the metrics before the bitmaps.

    Deliberately *not* also requiring `first >= 0x20`. That guard looks safe --
    the games only draw ASCII -- but LatinPlain12's first section is
    U+0002..U+0002, so it aborted that strike's walk at the first entry and
    silently reported the face as having no ASCII at all. A plausibility check
    that encodes an assumption about the data rather than about the format is
    how a parser fails quietly on the one input that differs.
    """
    for candidate in SECTION_TABLE_CANDIDATES:
        sections = _walk_sections(data, strike["recordOffset"] + candidate)
        # A real table opens with the ASCII range; anything else means the
        # offset is wrong for this build rather than that the font lacks ASCII.
        if sections and any(s["first"] == 0x20 for s in sections[:2]):
            # Where the table was found says which build this is, and so how
            # the metrics blocks the sections point at must be read.
            strike["sectionTableOffset"] = candidate
            return sections
    strike["sectionTableOffset"] = SECTION_TABLE_CANDIDATES[0]
    return _walk_sections(data, strike["recordOffset"] + SECTION_TABLE_CANDIDATES[0])


def is_shipping_layout(strike):
    return strike.get("sectionTableOffset") == SHIPPING_SECTION_TABLE_OFFSET


def _walk_sections(data, offset):
    sections = []
    while offset + SECTION_ENTRY_SIZE <= len(data):
        first, last = struct.unpack_from("<HH", data, offset)
        metrics_at, bitmap_at = struct.unpack_from("<II", data, offset + 4)
        plausible = (first <= last
                     and FIRST_RECORD_OFFSET < metrics_at < len(data)
                     and FIRST_RECORD_OFFSET < bitmap_at <= len(data)
                     and metrics_at < bitmap_at)
        if not plausible:
            break
        sections.append({
            "first": first,
            "last": last,
            "metricsOffset": metrics_at,
            "bitmapOffset": bitmap_at,
        })
        offset += SECTION_ENTRY_SIZE
    return sections


def read_metrics(data, section, strike):
    """Per-character metrics for one code section, keyed by code point.

    The block opens with its own character count, which is checked against the
    range the section table declared: the two agreeing is what says the section
    table and the metrics block are being read consistently rather than each
    plausibly on its own.
    """
    at = section["metricsOffset"]
    count = struct.unpack_from("<I", data, at)[0]
    expected = section["last"] - section["first"] + 1
    if count != expected:
        raise ValueError("section U+%04X..U+%04X declares %d characters but its "
                         "metrics block holds %d" % (section["first"], section["last"],
                                                     expected, count))
    at += 4
    if is_shipping_layout(strike):
        return _read_shipping_metrics(data, section, strike, at, count)
    metrics = {}
    for n in range(count):
        record = data[at + n * METRIC_SIZE:at + (n + 1) * METRIC_SIZE]
        bitmap_offset = struct.unpack_from("<H", record, 0)[0]
        metrics[section["first"] + n] = {
            "bitmapOffset": bitmap_offset,
            "ascent": record[2],
            "height": record[3],
            "leftAdjust": record[4],
            "move": record[5],
            "rightAdjust": record[6],
        }
    return metrics


def read_shared_metrics(data, strike):
    """The shipping build's per-strike metric table, as a list of dicts.

    The record names both the table's offset and its entry count; the table
    repeats the count in its own first word. Both must agree, which is the
    check that the record is being read at the right layout.
    """
    table_at, count = struct.unpack_from(
        "<II", data, strike["recordOffset"] + SHIPPING_METRIC_TABLE_FIELD)
    stored = struct.unpack_from("<I", data, table_at)[0]
    if stored != count:
        raise ValueError("record says the metric table at 0x%X has %d entries, the "
                         "table itself says %d" % (table_at, count, stored))
    entries = []
    at = table_at + 4
    for n in range(count):
        ascent, height, left, move, right = data[at + n * SHIPPING_METRIC_SIZE:
                                                 at + (n + 1) * SHIPPING_METRIC_SIZE]
        entries.append({"ascent": ascent, "height": height, "leftAdjust": left,
                        "move": move, "rightAdjust": right})
    return entries


def _read_shipping_metrics(data, section, strike, at, count):
    table = read_shared_metrics(data, strike)
    offsets = [struct.unpack_from("<H", data, at + 2 * n)[0] for n in range(count)]

    # The offset array must run exactly up to the bitmap block, and that block
    # opens with its own length, which bounds every offset. A parser that had
    # the array or the block start wrong would fail here rather than read the
    # wrong glyph's index byte and land on a plausible entry.
    bitmap_at = section["bitmapOffset"]
    if at + 2 * count != bitmap_at:
        raise ValueError("section U+%04X..U+%04X: offset array ends at 0x%X but the "
                         "bitmap block starts at 0x%X" % (section["first"], section["last"],
                                                          at + 2 * count, bitmap_at))
    length = struct.unpack_from("<I", data, bitmap_at)[0]
    glyphs_at = bitmap_at + 4
    if any(b >= a for a, b in zip(offsets[1:], offsets[:-1])) or offsets[-1] >= length:
        raise ValueError("section U+%04X..U+%04X: glyph offsets are not increasing "
                         "within the %d-byte bitmap block" % (section["first"],
                                                              section["last"], length))
    ends = offsets[1:] + [length]

    metrics = {}
    for n in range(count):
        glyph = data[glyphs_at + offsets[n]:glyphs_at + ends[n]]
        index = glyph[0] >> 1
        skip = 1
        if glyph[0] & 1:
            index += glyph[1] * 128
            skip = 2
        if index >= len(table):
            raise ValueError("U+%04X selects metric entry %d of %d"
                             % (section["first"] + n, index, len(table)))
        entry = dict(table[index])
        rows = decode_glyph(glyph[skip:], entry)
        entry.update({"bitmapOffset": offsets[n], "metricIndex": index, "rows": rows})
        metrics[section["first"] + n] = entry
    return metrics


def decode_glyph(stream, metric):
    """The glyph's rows, each a list of 0/1 columns, from its row-run stream.

    Every glyph must consume its bytes exactly -- the stream ends inside the
    last byte -- which is what makes this a check rather than an assumption:
    a wrong metric entry gives the wrong row width, and the run headers then
    fall on bits that are not headers.
    """
    width = metric["move"] - metric["leftAdjust"] - metric["rightAdjust"]
    height = metric["height"]
    if width <= 0 or height == 0:
        if stream:
            raise ValueError("a blank glyph carries %d bytes of bitmap" % len(stream))
        return []
    bits = [(byte >> k) & 1 for byte in stream for k in range(8)]
    rows = []
    at = 0
    while len(rows) < height:
        if at + 5 > len(bits):
            raise ValueError("glyph stream ends after %d of %d rows" % (len(rows), height))
        stored_each = bits[at]
        run = bits[at + 1] | bits[at + 2] << 1 | bits[at + 3] << 2 | bits[at + 4] << 3
        at += 5
        if run == 0 or len(rows) + run > height:
            raise ValueError("glyph run of %d rows at row %d of %d" % (run, len(rows), height))
        if stored_each:
            for _ in range(run):
                rows.append(bits[at:at + width])
                at += width
        else:
            rows.extend([bits[at:at + width]] * run)
            at += width
    if at > len(bits) or at <= len(bits) - 8:
        raise ValueError("glyph stream uses %d of %d bits" % (at, len(bits)))
    return rows


def check_height_matches_name(name, height):
    """The self-check: LatinBoldNN must have max height NN.

    The number in the name is the strike's total height, not its ascent -- the
    two header bytes are `height, ascent` in that order. That was established
    by cross-checking the header against the per-character metrics, where
    LatinBold12's header pair (12, 10) matches the per-character maxima
    (height 12, ascent 10) and not the other way round. All five Latin strikes
    agree, so the order is pinned by data rather than by the field names in
    `fntstore.h`, which describe the runtime class and not this record.

    Returns None when the name carries no trailing number to check against,
    so faces like Acp5 neither confirm nor deny the layout.
    """
    match = re.search(r"(\d+)$", name)
    if not match:
        return None
    return int(match.group(1)) == height


def find_ascii_metrics(data, strikes, wanted):
    """The named strike, its sections, and its ASCII metrics -- or an error."""
    match = None
    for strike in strikes:
        if read_name_for(data, strike["uid"]) == wanted:
            match = strike
            break
    if match is None:
        names = ", ".join(read_name_for(data, s["uid"]) or "?" for s in strikes)
        sys.stderr.write("no typeface %r; this store has: %s\n" % (wanted, names))
        return None

    sections = read_sections(data, match)
    ascii_section = None
    for section in sections:
        if section["first"] == 0x20:
            ascii_section = section
            break
    if ascii_section is None:
        sys.stderr.write("%s has no section starting at U+0020\n" % wanted)
        return None
    return match, sections, read_metrics(data, ascii_section, match)


def dump_glyphs(data, strikes, argv):
    """The device's own pixels for one strike, as text, for looking at.

    Only the shipping layout is decoded: it is the layout whose glyph stream
    was read out of the device's reader and whose every glyph passes the
    exact-fill check in `decode_glyph`. The SDK beta's bitmaps have not been
    verified the same way and are refused rather than guessed at.
    """
    wanted = argv[argv.index("--glyphs") + 1]
    found = find_ascii_metrics(data, strikes, wanted)
    if found is None:
        return 1
    strike, _sections, metrics = found
    if not is_shipping_layout(strike):
        sys.stderr.write("%s is in the SDK beta layout, whose glyph encoding is "
                         "not verified; only the shipping layout is decoded\n" % wanted)
        return 1
    text = argv[argv.index("--text") + 1] if "--text" in argv else None
    codes = [ord(c) for c in text] if text else sorted(metrics)

    # Lay the glyphs out on a shared baseline, each advanced by its own `move`,
    # so the preview shows spacing as well as shape.
    ascent = strike["ascent"]
    canvas_height = strike["height"]
    width = sum(metrics[c]["move"] for c in codes if c in metrics) + 1
    canvas = [["." for _ in range(width)] for _ in range(canvas_height)]
    x = 0
    for code in codes:
        m = metrics.get(code)
        if m is None:
            continue
        for r, row in enumerate(m["rows"]):
            y = ascent - m["ascent"] + r
            for cidx, bit in enumerate(row):
                if bit and 0 <= y < canvas_height:
                    canvas[y][x + m["leftAdjust"] + cidx] = "#"
        x += m["move"]
    sys.stdout.write("%s: %dpx, ascent %d\n" % (wanted, canvas_height, ascent))
    for row in canvas:
        sys.stdout.write("".join(row) + "\n")
    return 0


def dump_widths(data, strikes, argv):
    """Per-character metrics for one strike's ASCII range.

    Only ASCII is emitted: it is what the games draw, and it is the range whose
    correctness can be checked against the shipped art. The other sections are
    listed so that what was skipped is visible rather than silent.
    """
    wanted = argv[argv.index("--widths") + 1]
    found = find_ascii_metrics(data, strikes, wanted)
    if found is None:
        return 1
    match, sections, metrics = found

    out = ["\t".join(["code", "char", "ascent", "height", "leftAdjust", "move",
                      "rightAdjust"])]
    for code in sorted(metrics):
        m = metrics[code]
        char = chr(code) if code != 0x20 else "SP"
        out.append("\t".join([
            "0x%02X" % code, char, str(m["ascent"]), str(m["height"]),
            str(m["leftAdjust"]), str(m["move"]), str(m["rightAdjust"]),
        ]))
    text = "\n".join(out) + "\n"

    if "--write" in argv:
        path = argv[argv.index("--write") + 1]
        open(path, "w", encoding="utf-8", newline="\n").write(text)
        sys.stdout.write("wrote %s\n" % path)
    else:
        sys.stdout.write(text)

    advances = sorted(set(m["move"] for m in metrics.values()))
    sys.stdout.write("\n%s: %d characters, advances %d..%d (%d distinct)\n"
                     % (wanted, len(metrics), advances[0], advances[-1], len(advances)))
    sys.stdout.write("sections in this strike: %s\n"
                     % ", ".join("U+%04X..U+%04X" % (s["first"], s["last"]) for s in sections))
    if is_shipping_layout(match):
        used = len(set(m["metricIndex"] for m in metrics.values()))
        sys.stdout.write("layout: shipping (shared metric table, %d entries used by ASCII; "
                         "every glyph stream fills its bytes exactly)\n" % used)
    else:
        sys.stdout.write("layout: SDK beta (8-byte per-character records)\n")

    if len(advances) == 1:
        sys.stderr.write("every character has the same advance -- that is not a "
                         "proportional font, so the metrics block is misread\n")
        return 1

    # The header's own maxima must bound what the per-character records hold.
    # This is the check that caught the header pair being (height, ascent)
    # rather than (ascent, height): read the other way round, LatinBold12's
    # header said ascent 12 while no character exceeded ascent 10, and said
    # height 10 while characters reached 12. Only one order survives.
    max_ascent = max(m["ascent"] for m in metrics.values())
    max_height = max(m["height"] for m in metrics.values())
    max_move = max(advances)
    problems = []
    if max_ascent > match["ascent"]:
        problems.append("a character ascends %d but the header says max %d"
                        % (max_ascent, match["ascent"]))
    if max_height > match["height"]:
        problems.append("a character is %d tall but the header says max %d"
                        % (max_height, match["height"]))
    if max_move > match["maxCharWidth"]:
        problems.append("a character advances %d but the header says max %d"
                        % (max_move, match["maxCharWidth"]))
    sys.stdout.write("header bounds: height %d >= %d, ascent %d >= %d, width %d >= %d\n"
                     % (match["height"], max_height, match["ascent"], max_ascent,
                        match["maxCharWidth"], max_move))
    if problems:
        for problem in problems:
            sys.stderr.write("HEADER DISAGREES WITH METRICS: %s\n" % problem)
        return 1
    return 0


def main(argv):
    if len(argv) < 2:
        sys.stderr.write(__doc__)
        return 2
    data = open(argv[1], "rb").read()

    if data[0x0C:0x10] != STORE_TAG:
        sys.stderr.write("not a Symbian font store: tag at 0x0c is %r, want %r\n"
                         % (data[0x0C:0x10], STORE_TAG))
        return 1
    store_uid = struct.unpack_from("<I", data, 0x18)[0]
    if store_uid >> 8 != STORE_UID_PREFIX:
        sys.stderr.write("unexpected font store UID 0x%08x\n" % store_uid)
        return 1

    strikes = read_strikes(data)

    if "--widths" in argv:
        return dump_widths(data, strikes, argv)
    if "--glyphs" in argv:
        return dump_glyphs(data, strikes, argv)

    rows = []
    checked = 0
    agreed = 0
    for strike in strikes:
        name = read_name_for(data, strike["uid"]) or "?"
        verdict = check_height_matches_name(name, strike["height"])
        if verdict is not None:
            checked += 1
            agreed += 1 if verdict else 0
        rows.append((name, strike, verdict))

    out = ["\t".join(["typeface", "uid", "posture", "strokeWeight", "isProportional",
                      "height", "ascent", "maxCharWidth", "maxNormalCharWidth"])]
    for name, s, _ in rows:
        out.append("\t".join([
            name, "0x%08x" % s["uid"], str(s["posture"]), str(s["strokeWeight"]),
            str(s["isProportional"]), str(s["height"]), str(s["ascent"]),
            str(s["maxCharWidth"]), str(s["maxNormalCharWidth"]),
        ]))
    text = "\n".join(out) + "\n"

    if "--write" in argv:
        path = argv[argv.index("--write") + 1]
        open(path, "w", encoding="utf-8", newline="\n").write(text)
        sys.stdout.write("wrote %s\n" % path)
    else:
        sys.stdout.write(text)

    sys.stdout.write("\n%d of %d named strikes agree that height == the number in the name\n"
                     % (agreed, checked))
    if checked and agreed != checked:
        sys.stderr.write("LAYOUT CHECK FAILED -- the field order below is not trustworthy\n")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
