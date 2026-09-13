// Reusing a text box for a new message must show that message from its start.
//
// `npcHelloUI_` is built once and reused for every NPC greeting in Stormhold.
// Reading a long speech scrolls it, which advances `windowTop_`; the next
// greeting calls `setBodyText` -> `rewrapTextBox`, which rebuilds the rows.
// Dawnstar never sees the consequences, because `bodyTextResetsScroll` puts
// `windowTop_` back to 0 first; Stormhold sets that flag false, so the scroll
// position from the *previous* message survives into the new one.
//
// Two bugs live here, and the second is why this file asserts the window's
// contents rather than merely its bounds:
//
//   - **Out of range.** Six rows inheriting a `windowTop_` of 1 gave a
//     `windowBottom_` of 6, and the first paint indexed `rows_[6]` of a
//     six-row array and threw.
//
//   - **In range but wrong.** Clamping only `windowBottom_` fixes the throw
//     and leaves the text unreadable: scrolled to the bottom of Helga's
//     speech, `windowTop_` is well past 0, so a short greeting after it
//     rendered its last two lines with the beginning already scrolled away.
//     A bounds-only assertion passes that happily, so the checks below pin
//     the first row shown and the number of rows shown.
//
// The rule the widget has to hold to: the window shows up to `kVisible` rows,
// ends on the last row at the latest, and starts at the top whenever the whole
// message fits.

#include <cstdio>
#include <string>

#include "src/common/game/menulist.hpp"
#include "src/common/runtime.hpp"

namespace {

int failures = 0;

// What the text box passes: eleven rows visible at a time.
const int32_t kVisible = 11;

// `rewrapTextBox`, reduced to the arithmetic under test. `staleTop` is the
// scroll position inherited from the message before; `lineCount` is the number
// of rows the new message wrapped to.
void rewrap(int32_t staleTop, int32_t lineCount, int32_t *top, int32_t *bottom) {
    const int32_t visible = min32(lineCount, kVisible);
    *top = max32(0, min32(staleTop, lineCount - visible));
    *bottom = menulist::windowBottom(*top, lineCount, kVisible);
}

void checkWindow(int32_t staleTop, int32_t lineCount, int32_t wantTop, int32_t wantBottom,
                 const std::string &what) {
    int32_t top = 0;
    int32_t bottom = 0;
    rewrap(staleTop, lineCount, &top, &bottom);
    if (top != wantTop || bottom != wantBottom) {
        std::printf("FAIL: %s: stale top %d over %d rows gave [%d,%d], want [%d,%d]\n",
                    what.c_str(), (int)staleTop, (int)lineCount, (int)top, (int)bottom,
                    (int)wantTop, (int)wantBottom);
        ++failures;
    }
}

// The window must never name a row the array does not have.  This is the
// crash, and on its own it is not enough -- see `shortTextStartsAtTheTop`.
void windowStaysInsideTheRows() {
    for (int32_t lineCount = 1; lineCount <= 40; ++lineCount) {
        for (int32_t staleTop = 0; staleTop <= 40; ++staleTop) {
            int32_t top = 0;
            int32_t bottom = 0;
            rewrap(staleTop, lineCount, &top, &bottom);
            if (top < 0 || bottom > lineCount - 1 || bottom < top) {
                std::printf("FAIL: %d rows, stale top %d: window [%d,%d] is not inside [0,%d]\n",
                            (int)lineCount, (int)staleTop, (int)top, (int)bottom,
                            (int)(lineCount - 1));
                ++failures;
            }
        }
    }
}

// The reported bug: a message that fits on screen must be shown from its first
// line, however far down the previous message was scrolled.  Clamping only the
// bottom leaves this showing the tail of a six-row greeting.
void shortTextStartsAtTheTop() {
    for (int32_t staleTop = 0; staleTop <= 30; ++staleTop) {
        checkWindow(staleTop, 6, 0, 5, "a six-row greeting after a scrolled speech");
    }
    checkWindow(4, 2, 0, 1, "a two-row message after scrolling down four");
    checkWindow(10, 11, 0, 10, "a message that exactly fills the window");
    checkWindow(0, 1, 0, 0, "a single row");
}

// The logged crash, by its numbers: index 6 into a length-6 array.
void theLoggedCrash() {
    int32_t top = 0;
    int32_t bottom = 0;
    rewrap(1, 6, &top, &bottom);
    if (bottom == 6) {
        std::printf("FAIL: the logged crash reproduces: windowBottom=6 over 6 rows\n");
        ++failures;
    }
    checkWindow(1, 6, 0, 5, "the logged greeting shows all six of its rows");
}

// Text longer than the window still scrolls, and a scroll position that is
// still valid for the new text is kept rather than thrown away.
void longTextStillScrolls() {
    checkWindow(0, 30, 0, 10, "thirty rows show eleven from the top");
    checkWindow(5, 30, 5, 15, "a scroll position the new text can honour is kept");
    checkWindow(19, 30, 19, 29, "the last full window ends on the last row");
    checkWindow(25, 30, 19, 29, "a scroll past the end is pulled back to the last window");
}

// However far down the window sits, it always shows a full screen of text when
// there is one to show. This is the property the two-lines-on-screen bug broke.
void windowIsAlwaysFullWhenItCanBe() {
    for (int32_t lineCount = 1; lineCount <= 40; ++lineCount) {
        for (int32_t staleTop = 0; staleTop <= 40; ++staleTop) {
            int32_t top = 0;
            int32_t bottom = 0;
            rewrap(staleTop, lineCount, &top, &bottom);
            const int32_t shown = bottom - top + 1;
            const int32_t want = min32(lineCount, kVisible);
            if (shown != want) {
                std::printf("FAIL: %d rows, stale top %d: showed %d rows, want %d\n",
                            (int)lineCount, (int)staleTop, (int)shown, (int)want);
                ++failures;
            }
        }
    }
}

}

int main() {
    windowStaysInsideTheRows();
    shortTextStartsAtTheTop();
    theLoggedCrash();
    longTextStillScrolls();
    windowIsAlwaysFullWhenItCanBe();
    if (failures != 0) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
