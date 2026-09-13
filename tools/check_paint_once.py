"""Fail if a canvas tick paints more than once on its normal path.

A tick must issue exactly one guarded `repaint(); serviceRepaints();` pair,
because the game's hit indicators are drawn-and-cleared in the same call:

    if (showBloodFx_) { drawIcon(...); showBloodFx_ = false; }

`drawEffects` clears each flag as it draws it, so the blood splash, the monster
spell flash and the self spell flash each live for exactly one painted frame.
Painting twice draws the effect, clears the flag, then immediately repaints the
same frame without it -- so every hit indicator is erased in the tick that
produced it, and combat has no visible feedback at all.

That is not hypothetical. Inverting the canvas loop in Phase 2 (`ece7733`)
restructured the loop body and emitted the guarded paint block twice, in both
games. It survived four commits and was found by a player noticing the damage
indicators had gone, not by a test -- which is why this file exists.

**No existing test could have caught it.** The frame baselines compare rendered
output at fixed points, and painting an otherwise-identical frame twice is
byte-identical, so the bug is invisible to them by construction. The replay
hashes cover state, not how often it was drawn. This is a property of the loop's
shape rather than of any output, so it is checked as one.

What is counted is deliberately narrow: paints guarded by `repaintEnabled_`,
which is the normal per-tick path. The canvas also paints from `catch` blocks
-- those are real and are left alone. (There is one more `catch` paint than in
the Java originals, because a single `catch (Throwable)` splits into
`catch (std::exception)` and `catch (...)`. That is faithful, not a divergence,
and is exactly the sort of thing a raw total would have flagged wrongly.)

The canvas has been one class for both games since Phase 5 of
docs/UNIFICATION_PLAN.md, so there is one file to check.

Usage: python tools/check_paint_once.py
Exits non-zero if a canvas gained a guarded paint.
"""

import io
import os
import re
import sys

CANVASES = (
    "src/common/game/game_canvas.cpp",
)

# `if (repaintEnabled_) { repaint(); serviceRepaints(); }` -- the normal
# per-tick paint, in whatever whitespace it is written.
GUARDED_PAINT = re.compile(
    r"if\s*\(\s*repaintEnabled_\s*\)\s*\{\s*"
    r"this->repaint\(\)\s*;\s*"
    r"this->serviceRepaints\(\)\s*;\s*\}")

EXPECTED = 1


def strip_comments(text):
    text = re.sub(r"//[^\n]*", "", text)
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def main():
    failures = []
    for path in CANVASES:
        if not os.path.isfile(path):
            failures.append("  %s: missing" % path)
            continue

        with io.open(path, encoding="utf-8", errors="replace") as handle:
            text = strip_comments(handle.read())

        found = len(GUARDED_PAINT.findall(text))
        if found != EXPECTED:
            failures.append("  %s: %d guarded paint(s), expected %d"
                            % (path, found, EXPECTED))

    if failures:
        print("Canvas paint counts have changed:")
        for line in failures:
            print(line)
        print("\nA tick must paint exactly once on its normal path.")
        print("drawEffects() clears each effect flag as it draws it, so a")
        print("second paint in the same tick erases the blood splash and both")
        print("spell flashes -- combat loses its only visual feedback.")
        return 1

    print("paint counts OK: one guarded paint in the canvas")
    return 0


if __name__ == "__main__":
    sys.exit(main())
