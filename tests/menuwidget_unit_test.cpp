// The two machines Phase 6 pulled out of the widgets: the splash sequence and
// the navigation key.
//
// Both were shared by *removing* a copy, so the risk is not that the shared
// version is wrong in the abstract -- it is that it is subtly not what one of
// the two ports was doing. The frame and menu baselines cover that end to end,
// and this file covers the parts of the contract those baselines pin only by
// accident of which screens they happen to visit:
//
//   - **The splash's beat counts.** `sleepRepainting(2000)` ran while its
//     accumulator was `<=` the target, so it repainted five times, not four.
//     A `<` there is off by one repaint in a sequence the frame baselines see
//     only at 500 ms sampling, so the count is asserted directly.
//
//   - **The four-second floor.** The first beat ends when the loader reports
//     100% *and* four seconds have passed. Either condition alone ending it is
//     a plausible misreading of `progress < 100 || elapsed < 4000`, and the
//     harnesses drive a loader that finishes fast, so they would not notice.
//
//   - **That `navigate` repaints only on a move that happened.** Both ports
//     returned early from the arithmetic and skipped the repaint; a shared
//     version that repaints unconditionally still passes every baseline,
//     because a baseline records the frame, not how many times it was drawn.
//
//   - **That the two selection shapes stay distinct.** `lineOf` empty is the
//     one-line-per-entry list; `lineOf` present is the wrapped one. The two
//     take different branches for the same key, and only Dawnstar exercises
//     the second.
//
// One injected mutation is deliberately *not* caught, named so the gap is not
// mistaken for an oversight. Writing the selection back unconditionally rather
// than only on a move is a true equivalent: `navigate` reads it into a local
// and the arithmetic leaves that local untouched when it returns false, so the
// write stores the value that was already there.

#include <cstdio>
#include <string>
#include <vector>

#include "src/common/game/menulist.hpp"
#include "src/common/game/splashphase.hpp"
#include "src/common/game/uistate.hpp"

namespace {

int failures = 0;

void check(bool ok, const std::string &what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    }
}

void checkEq(int32_t got, int32_t want, const std::string &what) {
    if (got != want) {
        std::printf("FAIL: %s: got %d, want %d\n", what.c_str(), (int)got, (int)want);
        ++failures;
    }
}

// ---------------------------------------------------------------- splash

// Records what the beats did, so the sequence can be asserted rather than
// watched. `repaints` is per phase, which is what makes the counts checkable.
struct SplashLog {
    int repaints[4] = {0, 0, 0, 0};
    int carrier = 0;
    int publisher = 0;
    int finished = 0;
    std::vector<splashphase::Phase> repaintPhases;
};

void logRepaint(void *ctx, splashphase::Phase phase) {
    SplashLog *log = (SplashLog *)ctx;
    ++log->repaints[(int)phase];
    log->repaintPhases.push_back(phase);
}
void logCarrier(void *ctx) { ++((SplashLog *)ctx)->carrier; }
void logPublisher(void *ctx) { ++((SplashLog *)ctx)->publisher; }
void logFinish(void *ctx) { ++((SplashLog *)ctx)->finished; }

splashphase::Hooks hooksFor(SplashLog *log) {
    splashphase::Hooks hooks;
    hooks.repaint = logRepaint;
    hooks.showCarrierLogo = logCarrier;
    hooks.showPublisherLogo = logPublisher;
    hooks.finish = logFinish;
    hooks.ctx = log;
    return hooks;
}

// Runs to completion at the 500 ms cadence both hosts use. Returns the number
// of steps taken.
int runSplash(SplashLog *log, int32_t progressPct, int guard = 200) {
    splashphase::State state;
    splashphase::Hooks hooks = hooksFor(log);
    int steps = 0;
    while (steps < guard) {
        ++steps;
        if (!splashphase::step(&state, hooks, 500, true, progressPct)) {
            return steps;
        }
    }
    return steps;
}

void testSplashSequence() {
    SplashLog log;
    int steps = runSplash(&log, 100);

    check(steps < 200, "splash terminates");
    checkEq(log.carrier, 1, "carrier logo raised exactly once");
    checkEq(log.publisher, 1, "publisher logo raised exactly once");
    checkEq(log.finished, 1, "finish runs exactly once");

    // The order is the contract; a machine that finished without passing
    // through both logos would still satisfy the counts above on its own.
    check(log.repaints[(int)splashphase::Phase::Progress] > 0, "progress phase repaints");
    bool sawCarrierAfterProgress = false;
    bool sawPublisherAfterCarrier = false;
    splashphase::Phase previous = splashphase::Phase::Progress;
    for (size_t n = 0; n < log.repaintPhases.size(); ++n) {
        splashphase::Phase phase = log.repaintPhases[n];
        if (previous == splashphase::Phase::Progress && phase == splashphase::Phase::CarrierLogo) {
            sawCarrierAfterProgress = true;
        }
        if (previous == splashphase::Phase::CarrierLogo &&
            phase == splashphase::Phase::PublisherLogo) {
            sawPublisherAfterCarrier = true;
        }
        previous = phase;
    }
    check(sawCarrierAfterProgress, "carrier logo follows the progress phase");
    check(sawPublisherAfterCarrier, "publisher logo follows the carrier logo");
}

void testSplashBeatCounts() {
    SplashLog log;
    runSplash(&log, 100);

    // sleepRepainting(2000) at 500 ms a step, run while `<= 2000`: five, not
    // four. The same reading gives the 1000 ms publisher beat three, not two.
    checkEq(log.repaints[(int)splashphase::Phase::CarrierLogo], 5,
            "carrier logo repaints five times, not four");
    checkEq(log.repaints[(int)splashphase::Phase::PublisherLogo], 3,
            "publisher logo repaints three times, not two");

    // And the first beat repaints on every one of its steps. Asserted as an
    // exact count rather than "more than none": the cadence gate is
    // `accum >= 500` and the hosts step at exactly 500, so a `>` there stops
    // every repaint in every phase -- which the counts above cannot see,
    // since zero repaints leaves their ordering vacuously satisfied.
    checkEq(log.repaints[(int)splashphase::Phase::Progress], 7,
            "the first beat repaints on each of its seven steps");
}

void testSplashFourSecondFloor() {
    // A loader that reports 100% immediately must still hold the first beat
    // for four seconds: the condition is `progress < 100 || elapsed < 4000`.
    // The elapsed time is accumulated before the test, so seven steps put it
    // at 3500 and the beat holds; the eighth reaches 4000 and `< 4000` fails.
    SplashLog log;
    splashphase::State state;
    splashphase::Hooks hooks = hooksFor(&log);
    for (int n = 0; n < 7; ++n) {
        splashphase::step(&state, hooks, 500, true, 100);
    }
    checkEq(log.carrier, 0, "first beat holds for four seconds even at 100%");
    checkEq((int32_t)state.phase, (int32_t)splashphase::Phase::Progress, "still on the first beat");
    splashphase::step(&state, hooks, 500, true, 100);
    checkEq(log.carrier, 1, "first beat ends once four seconds have passed");
}

void testSplashWaitsForLoader() {
    // And symmetrically: four seconds is not enough on its own.
    SplashLog log;
    splashphase::State state;
    splashphase::Hooks hooks = hooksFor(&log);
    for (int n = 0; n < 40; ++n) {
        splashphase::step(&state, hooks, 500, true, 40);
    }
    checkEq(log.carrier, 0, "first beat waits for the loader however long it takes");
}

void testSplashStopFlag() {
    // stopThread() clears the flag; the loop fell out of its wait immediately.
    SplashLog log;
    splashphase::State state;
    splashphase::Hooks hooks = hooksFor(&log);
    splashphase::step(&state, hooks, 500, false, 0);
    checkEq(log.carrier, 1, "a cleared running flag ends the first beat at once");
}

void testSplashDoneIsTerminal() {
    SplashLog log;
    splashphase::State state;
    state.phase = splashphase::Phase::Done;
    splashphase::Hooks hooks = hooksFor(&log);
    check(!splashphase::step(&state, hooks, 500, true, 100), "Done reports nothing left to do");
    checkEq(log.finished, 0, "Done does not finish twice");
}

// ------------------------------------------------------------ splash skip

// Pressing confirm cuts the logos short. The case worth pinning is the one it
// deliberately does *not* cut short: Progress is the loader, and finishing it
// early would show the menu over half-built state.

void testSkipEndsTheLogos() {
    SplashLog log;
    splashphase::State state;
    splashphase::Hooks hooks = hooksFor(&log);

    // Advance into the carrier logo. Progress ends only once the loader reports
    // 100% *and* the four-second floor has passed, so this takes nine steps.
    while (state.phase == splashphase::Phase::Progress) {
        splashphase::step(&state, hooks, 500, true, 100);
    }
    check(state.phase == splashphase::Phase::CarrierLogo, "reached the carrier logo");

    check(splashphase::skip(&state, hooks), "skip consumes the key");
    check(state.phase == splashphase::Phase::Done, "skip lands on Done");
    checkEq(log.finished, 1, "skip finishes exactly once");

    // And the machine stays finished: another step must not re-run finish.
    check(!splashphase::step(&state, hooks, 500, true, 100), "Done stays done");
    checkEq(log.finished, 1, "finish does not run twice");
}

void testSkipFromPublisherLogo() {
    SplashLog log;
    splashphase::State state;
    state.phase = splashphase::Phase::PublisherLogo;
    splashphase::Hooks hooks = hooksFor(&log);

    check(splashphase::skip(&state, hooks), "skip works from the second logo too");
    checkEq(log.finished, 1, "and finishes once");
}

void testSkipDoesNotCutTheLoaderShort() {
    SplashLog log;
    splashphase::State state;
    splashphase::Hooks hooks = hooksFor(&log);

    // Still loading: 40% done, nowhere near the four-second floor.
    splashphase::step(&state, hooks, 500, true, 40);
    check(state.phase == splashphase::Phase::Progress, "still loading");

    check(!splashphase::skip(&state, hooks), "skip does not consume the key while loading");
    check(state.phase == splashphase::Phase::Progress, "skip leaves the loader alone");
    checkEq(log.finished, 0, "and does not finish early");
    checkEq(log.carrier, 0, "nor jump to a logo");

    // The press is dropped, not queued: the loader still has to finish and
    // clear the four-second floor on its own.
    splashphase::step(&state, hooks, 500, true, 100);
    check(state.phase == splashphase::Phase::Progress,
          "the four-second floor still applies");
}

void testSkipOnDoneIsHarmless() {
    SplashLog log;
    splashphase::State state;
    state.phase = splashphase::Phase::Done;
    splashphase::Hooks hooks = hooksFor(&log);

    // This is the regression that broke menu selection: the splash widget
    // outlives the intro, so a skip once Done must NOT claim the key -- the
    // front end would swallow every confirm press for the rest of the session.
    check(!splashphase::skip(&state, hooks), "skip on Done does not consume the key");
    checkEq(log.finished, 0, "and does not finish a second time");
}

// ------------------------------------------------------------- navigate

// Stands in for either widget's selection storage.
struct NavLog {
    int32_t selected = 0;
    int32_t entries = 0;
    int repaints = 0;
    int reads = 0;
};

int32_t navRead(void *ctx) {
    ++((NavLog *)ctx)->reads;
    return ((NavLog *)ctx)->selected;
}
void navWrite(void *ctx, int32_t index) { ((NavLog *)ctx)->selected = index; }
int32_t navCount(void *ctx) { return ((NavLog *)ctx)->entries; }
void navRepaint(void *ctx) { ++((NavLog *)ctx)->repaints; }

menulist::NavigationView screenFor(NavLog *log, int32_t layout) {
    menulist::NavigationView screen;
    screen.layout = layout;
    screen.selectedIndex = navRead;
    screen.setSelectedIndex = navWrite;
    screen.entryCount = navCount;
    screen.repaint = navRepaint;
    screen.ctx = log;
    return screen;
}

const int32_t ACTION_UP = 1;
const int32_t ACTION_DOWN = 6;

void testNavigateMovesAndRepaints() {
    NavLog log;
    log.entries = 3;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_LIST);
    screen.lineCount = 3;
    int32_t first = 0;
    int32_t last = 2;

    menulist::navigate(screen, ACTION_DOWN, &first, &last);
    checkEq(log.selected, 1, "down moves the selection");
    checkEq(log.repaints, 1, "a move repaints");

    menulist::navigate(screen, ACTION_UP, &first, &last);
    checkEq(log.selected, 0, "up moves it back");
    checkEq(log.repaints, 2, "and repaints again");
}

void testNavigateSilentAtTheEnds() {
    // The reason a caller cannot just repaint unconditionally: both ports
    // returned early, and a baseline records the frame, not the draw count.
    NavLog log;
    log.entries = 2;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_LIST);
    screen.lineCount = 2;
    int32_t first = 0;
    int32_t last = 1;

    menulist::navigate(screen, ACTION_UP, &first, &last);
    checkEq(log.repaints, 0, "up on the first entry repaints nothing");
    checkEq(log.selected, 0, "and moves nothing");

    log.selected = 1;
    menulist::navigate(screen, ACTION_DOWN, &first, &last);
    checkEq(log.repaints, 0, "down on the last entry repaints nothing");
    checkEq(log.selected, 1, "and moves nothing");
}

void testNavigateIgnoresOtherKeys() {
    NavLog log;
    log.entries = 3;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_LIST);
    screen.lineCount = 3;
    int32_t first = 0;
    int32_t last = 2;
    for (int32_t action = 0; action < 10; ++action) {
        if (action == ACTION_UP || action == ACTION_DOWN) continue;
        menulist::navigate(screen, action, &first, &last);
    }
    checkEq(log.repaints, 0, "only actions 1 and 6 do anything");
    checkEq(log.selected, 0, "and nothing else moves the selection");
    checkEq(log.reads, 0, "nor reads the selection");
}

void testNavigateScrollsTextBox() {
    // No selection on a text box: the window moves on its own, bounded by the
    // line count rather than by an entry count.
    NavLog log;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_TEXTBOX);
    screen.lineCount = 5;
    int32_t first = 0;
    int32_t last = 2;

    menulist::navigate(screen, ACTION_DOWN, &first, &last);
    checkEq(first, 1, "the window scrolls down");
    checkEq(last, 3, "keeping its height");
    checkEq(log.selected, 0, "without touching a selection");

    menulist::navigate(screen, ACTION_UP, &first, &last);
    checkEq(first, 0, "and back up");
    checkEq(last, 2, "still keeping its height");

    menulist::navigate(screen, ACTION_UP, &first, &last);
    checkEq(first, 0, "stopping at the top");
    checkEq(log.repaints, 2, "and repainting only for the two that moved");
}

void testTextBoxUpStillReadsTheSelection() {
    // Preserved deliberately: both originals read the selection on the up key
    // of a text box and dropped it, so the callback must still be observed.
    NavLog log;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_TEXTBOX);
    screen.lineCount = 5;
    int32_t first = 1;
    int32_t last = 3;
    menulist::navigate(screen, ACTION_UP, &first, &last);
    checkEq(log.reads, 1, "the up key still reads the selection on a text box");

    log.reads = 0;
    menulist::navigate(screen, ACTION_DOWN, &first, &last);
    checkEq(log.reads, 0, "the down key does not");
}

void testNavigateWrappedEntries() {
    // The other list shape: entry 1 wraps onto two display lines, so the
    // window is measured in lines and stepping onto the last entry pins it to
    // the end rather than scrolling by one.
    NavLog log;
    log.entries = 3;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_FORM_1);
    SharedArray<int32_t> lineOf(3);
    lineOf[0] = 0;
    lineOf[1] = 1;
    lineOf[2] = 3;  // entry 1 took lines 1 and 2
    screen.lineOf = lineOf;
    screen.lineCount = 4;
    int32_t first = 0;
    int32_t last = 1;

    menulist::navigate(screen, ACTION_DOWN, &first, &last);
    checkEq(log.selected, 1, "down selects the wrapped entry");
    // The next entry starts at line 3, past the window, so the window moves
    // only far enough to show it -- which is what keeps the entry whole.
    checkEq(last, 3, "the window follows to the next entry's first line");
    checkEq(first, 2, "keeping its height");

    menulist::navigate(screen, ACTION_DOWN, &first, &last);
    checkEq(log.selected, 2, "down selects the last entry");
    checkEq(last, 3, "the window pins to the end of the list");
    checkEq(first, 2, "still keeping its height");

    menulist::navigate(screen, ACTION_DOWN, &first, &last);
    checkEq(log.selected, 2, "and stops there");
}

void testNavigateLayoutsThatDoNothing() {
    // The splash and progress layouts have no list and no window.
    const int32_t inert[] = {uistate::LAYOUT_DOWNLOAD, uistate::LAYOUT_SPLASH,
                          uistate::LAYOUT_PROGRESS_NEW_GAME, uistate::LAYOUT_PROGRESS_LOAD_GAME,
                          uistate::LAYOUT_PROGRESS_SAVE_GAME,
                          uistate::LAYOUT_PROGRESS_LOAD_DUNGEON};
    for (size_t n = 0; n < sizeof(inert) / sizeof(inert[0]); ++n) {
        std::string what = "layout " + std::to_string((int)inert[n]);
        NavLog log;
        log.entries = 3;
        menulist::NavigationView screen = screenFor(&log, inert[n]);
        screen.lineCount = 5;
        // Deliberately mid-window, with room to move in both directions: a
        // window already pinned at both ends would sit still whatever branch
        // it took, so the fixture would prove nothing about which one ran.
        int32_t first = 1;
        int32_t last = 3;
        menulist::navigate(screen, ACTION_DOWN, &first, &last);
        menulist::navigate(screen, ACTION_UP, &first, &last);
        checkEq(log.repaints, 0, what + " ignores navigation");
        checkEq(first, 1, what + " leaves the window alone");
        checkEq(last, 3, what + " leaves the window height alone");
        checkEq(log.selected, 0, what + " leaves the selection alone");
    }
}

void testBothFormLayoutsAreListShaped() {
    // FORM_1 and FORM_2 differ only in whether a subtitle is drawn; both carry
    // a selectable list, and both must navigate like one.
    const int32_t forms[] = {uistate::LAYOUT_LIST, uistate::LAYOUT_FORM_1, uistate::LAYOUT_FORM_2};
    for (size_t n = 0; n < sizeof(forms) / sizeof(forms[0]); ++n) {
        std::string what = "layout " + std::to_string((int)forms[n]);
        NavLog log;
        log.entries = 3;
        menulist::NavigationView screen = screenFor(&log, forms[n]);
        screen.lineCount = 3;
        int32_t first = 0;
        int32_t last = 2;
        menulist::navigate(screen, ACTION_DOWN, &first, &last);
        checkEq(log.selected, 1, what + " moves its selection");
        checkEq(first, 0, what + " does not scroll a window that already fits");
        checkEq(log.repaints, 1, what + " repaints once");
    }
}

// ----------------------------------------------------------------- jump

// Home/End/PageUp/PageDown are port additions, so nothing in the baselines
// covers them. What is worth pinning is that they reuse moveUp/moveDown rather
// than recomputing the window: the wrapped-entry case below is the one a
// hand-rolled "set selected = count - 1" would get wrong, because first/last
// have to follow the variable number of lines each entry occupies.

void testJumpHomeAndEnd() {
    NavLog log;
    log.entries = 20;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_LIST);
    screen.lineCount = 20;
    int32_t first = 0;
    int32_t last = 9;

    menulist::jump(screen, shortcuts::JUMP_END, 10, &first, &last);
    checkEq(log.selected, 19, "End selects the last entry");
    checkEq(first, 10, "End scrolls the window to the bottom");
    checkEq(last, 19, "keeping its height");
    checkEq(log.repaints, 1, "a jump repaints exactly once");

    menulist::jump(screen, shortcuts::JUMP_HOME, 10, &first, &last);
    checkEq(log.selected, 0, "Home selects the first entry");
    checkEq(first, 0, "Home scrolls the window to the top");
    checkEq(last, 9, "keeping its height");
    checkEq(log.repaints, 2, "and repaints once more");
}

void testJumpPaging() {
    NavLog log;
    log.entries = 20;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_LIST);
    screen.lineCount = 20;
    int32_t first = 0;
    int32_t last = 9;

    // A page is one line short of the window, so the entry you were looking at
    // stays on screen as an anchor.
    menulist::jump(screen, shortcuts::JUMP_PAGE_DOWN, 10, &first, &last);
    checkEq(log.selected, 9, "PageDown advances a window minus one");

    menulist::jump(screen, shortcuts::JUMP_PAGE_UP, 10, &first, &last);
    checkEq(log.selected, 0, "PageUp comes back the same distance");
}

void testJumpStopsAtTheEnds() {
    NavLog log;
    log.entries = 3;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_LIST);
    screen.lineCount = 3;
    int32_t first = 0;
    int32_t last = 2;

    menulist::jump(screen, shortcuts::JUMP_HOME, 10, &first, &last);
    checkEq(log.repaints, 0, "Home on the first entry repaints nothing");
    checkEq(log.selected, 0, "and moves nothing");

    menulist::jump(screen, shortcuts::JUMP_PAGE_DOWN, 10, &first, &last);
    checkEq(log.selected, 2, "PageDown past the end stops at the last entry");
    checkEq(log.repaints, 1, "having moved, it repaints once");

    menulist::jump(screen, shortcuts::JUMP_END, 10, &first, &last);
    checkEq(log.repaints, 1, "End on the last entry repaints nothing");
}

void testJumpWrappedEntries() {
    NavLog log;
    log.entries = 3;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_FORM_1);
    SharedArray<int32_t> lineOf(3);
    lineOf[0] = 0;
    lineOf[1] = 1;
    lineOf[2] = 3;  // entry 1 took lines 1 and 2
    screen.lineOf = lineOf;
    screen.lineCount = 4;
    int32_t first = 0;
    int32_t last = 1;

    menulist::jump(screen, shortcuts::JUMP_END, 2, &first, &last);
    checkEq(log.selected, 2, "End reaches the last wrapped entry");
    checkEq(last, 3, "and the window follows the wrapped lines");
    checkEq(first, 2, "keeping its height");
}

void testJumpScrollsTextBox() {
    NavLog log;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_TEXTBOX);
    screen.lineCount = 10;
    int32_t first = 0;
    int32_t last = 2;

    menulist::jump(screen, shortcuts::JUMP_END, 3, &first, &last);
    checkEq(first, 7, "End scrolls a text box to the bottom");
    checkEq(last, 9, "keeping its height");
    checkEq(log.selected, 0, "without touching a selection");

    menulist::jump(screen, shortcuts::JUMP_HOME, 3, &first, &last);
    checkEq(first, 0, "Home scrolls a text box back to the top");
    checkEq(last, 2, "keeping its height");
}

void testJumpIgnoresOrdinaryActions() {
    NavLog log;
    log.entries = 5;
    menulist::NavigationView screen = screenFor(&log, uistate::LAYOUT_LIST);
    screen.lineCount = 5;
    int32_t first = 0;
    int32_t last = 4;
    for (int32_t action = 0; action < 10; ++action) {
        menulist::jump(screen, action, 5, &first, &last);
    }
    checkEq(log.repaints, 0, "jump does nothing for the MIDP game actions");
    checkEq(log.selected, 0, "and moves nothing");
}

}  // namespace

int main() {
    testSplashSequence();
    testSplashBeatCounts();
    testSplashFourSecondFloor();
    testSplashWaitsForLoader();
    testSplashStopFlag();
    testSplashDoneIsTerminal();

    testSkipEndsTheLogos();
    testSkipFromPublisherLogo();
    testSkipDoesNotCutTheLoaderShort();
    testSkipOnDoneIsHarmless();

    testNavigateMovesAndRepaints();
    testNavigateSilentAtTheEnds();
    testNavigateIgnoresOtherKeys();
    testNavigateScrollsTextBox();
    testTextBoxUpStillReadsTheSelection();
    testNavigateWrappedEntries();
    testNavigateLayoutsThatDoNothing();
    testBothFormLayoutsAreListShaped();

    testJumpHomeAndEnd();
    testJumpPaging();
    testJumpStopsAtTheEnds();
    testJumpWrappedEntries();
    testJumpScrollsTextBox();
    testJumpIgnoresOrdinaryActions();

    if (failures != 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("menuwidget: all checks passed\n");
    return 0;
}
