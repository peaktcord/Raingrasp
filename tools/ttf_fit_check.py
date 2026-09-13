"""Fit an outline font's advance widths against the extracted bitmap strikes.

Why this exists
---------------
`tools/gdr_dump.py` reads per-character advances out of `CEUROPE.GDR`, and the
checks there confirm the *format* is read correctly. They say nothing about
whether that file is the typeface a Nokia 3650 actually drew MIDlet text with.

KEmulator ships `res/s60snr.ttf` -- Series 60 Sans Regular, the outline typeface
the device's bitmap strikes were compiled from. It is a second artifact of the
same family that knows nothing about the GDR, so fitting one against the other
is an independent check on the extraction.

What a good result looks like, and what it does not
---------------------------------------------------
The expected pattern is that **LatinPlain12 fits best**: s60snr.ttf is the
*Regular* weight, so the plain strike is the only one with a like-for-like
outline, and the bold strikes should fit worse because nothing bold is being
compared. That pattern falling out the right way round is the corroboration.

It is not proof, and the exact-match percentage should not be read as an
accuracy score. Bitmap strikes are hand-hinted; a naively scaled outline cannot
reproduce that hinting, so perfect agreement would be surprising rather than
reassuring. This confirms the *family*, not the device.

The scale is fitted rather than assumed: pixels-per-em is swept and the value
minimising squared error is reported, because the relationship between a
strike's nominal height and the em size it was hinted from is not recorded
anywhere in either file.

Usage: python tools/ttf_fit_check.py <s60snr.ttf> [strike-name ...]
"""

import csv
import os
import struct
import sys

SENTINEL_DIR = os.path.join("validation", "sentinels")
DEFAULT_STRIKES = ["LatinPlain12", "LatinBold12", "LatinBold13", "LatinBold17",
                   "LatinBold19"]


class Outline:
    """Just enough TrueType to read advance widths by character."""

    def __init__(self, path):
        self.data = open(path, "rb").read()
        count = struct.unpack_from(">H", self.data, 4)[0]
        self.tables = {}
        for n in range(count):
            record = 12 + n * 16
            tag = self.data[record:record + 4].decode("latin1")
            self.tables[tag] = struct.unpack_from(">I", self.data, record + 8)[0]
        self.units_per_em = struct.unpack_from(">H", self.data, self.tables["head"] + 18)[0]
        self.metric_count = struct.unpack_from(">H", self.data, self.tables["hhea"] + 34)[0]
        self._load_cmap()

    def _load_cmap(self):
        data = self.data
        base = self.tables["cmap"]
        subtable = None
        for n in range(struct.unpack_from(">H", data, base + 2)[0]):
            platform, encoding, offset = struct.unpack_from(">HHI", data, base + 4 + n * 8)
            if (platform, encoding) in ((3, 1), (0, 3), (0, 4)):
                subtable = base + offset
        if subtable is None:
            raise ValueError("no Unicode cmap subtable")
        seg_x2 = struct.unpack_from(">H", data, subtable + 6)[0]
        segments = seg_x2 // 2
        self._ends = [struct.unpack_from(">H", data, subtable + 14 + n * 2)[0]
                      for n in range(segments)]
        self._starts = [struct.unpack_from(">H", data, subtable + 16 + seg_x2 + n * 2)[0]
                        for n in range(segments)]
        self._deltas = [struct.unpack_from(">h", data, subtable + 16 + 2 * seg_x2 + n * 2)[0]
                        for n in range(segments)]
        self._range_base = subtable + 16 + 3 * seg_x2
        self._ranges = [struct.unpack_from(">H", data, self._range_base + n * 2)[0]
                        for n in range(segments)]
        self._segments = segments

    def glyph_for(self, code):
        for n in range(self._segments):
            if self._starts[n] <= code <= self._ends[n]:
                if self._ranges[n] == 0:
                    return (code + self._deltas[n]) & 0xFFFF
                at = self._range_base + n * 2 + self._ranges[n] + (code - self._starts[n]) * 2
                glyph = struct.unpack_from(">H", self.data, at)[0]
                return (glyph + self._deltas[n]) & 0xFFFF if glyph else 0
        return 0

    def advance(self, code):
        glyph = self.glyph_for(code)
        if glyph == 0:
            return None
        index = glyph if glyph < self.metric_count else self.metric_count - 1
        return struct.unpack_from(">H", self.data, self.tables["hmtx"] + index * 4)[0]


def load_strike(name):
    path = os.path.join(SENTINEL_DIR, "s60-metrics-%s.tsv" % name)
    with open(path, encoding="utf-8") as handle:
        return list(csv.DictReader(handle, delimiter="\t", quoting=csv.QUOTE_NONE))


def fit(outline, rows):
    """Best pixels-per-em by least squared error, and how it scores there."""
    best = None
    for tenths in range(60, 260):
        ppem = tenths / 10.0
        error = 0
        compared = 0
        exact = 0
        for row in rows:
            code = int(row["code"], 16)
            if code == 0x20:  # space carries no outline to compare
                continue
            advance = outline.advance(code)
            if advance is None:
                continue
            predicted = round(advance / outline.units_per_em * ppem)
            actual = int(row["move"])
            error += (predicted - actual) ** 2
            compared += 1
            exact += predicted == actual
        if compared and (best is None or error < best[1]):
            best = (ppem, error, compared, exact)
    return best


def main(argv):
    if len(argv) < 2:
        sys.stderr.write(__doc__)
        return 2
    outline = Outline(argv[1])
    strikes = argv[2:] or DEFAULT_STRIKES

    sys.stdout.write("%-14s %9s %10s %14s\n" % ("strike", "ppem", "rms px", "exact"))
    results = {}
    for name in strikes:
        best = fit(outline, load_strike(name))
        if best is None:
            sys.stdout.write("%-14s  no comparable characters\n" % name)
            continue
        ppem, error, compared, exact = best
        rms = (error / compared) ** 0.5
        results[name] = rms
        sys.stdout.write("%-14s %9.1f %10.2f %8d/%d (%.0f%%)\n"
                         % (name, ppem, rms, exact, compared, 100.0 * exact / compared))

    if "LatinPlain12" in results and len(results) > 1:
        others = [rms for name, rms in results.items() if name != "LatinPlain12"]
        if results["LatinPlain12"] <= min(others):
            sys.stdout.write("\nLatinPlain12 fits best, as expected against a Regular-weight "
                             "outline.\n")
        else:
            sys.stdout.write("\nUNEXPECTED: a bold strike fits the Regular outline better than "
                             "the plain one.\nThat is the wrong way round, so either the "
                             "extraction or the pairing is suspect.\n")
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
