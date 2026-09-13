// The clock and PlatformContext pacing seams must be drivable
// by the host.
//
// Phase 2 makes the game a state machine the host ticks, and Layer 3 replay
// then needs a run to be a pure function of (seed, variant, input script). Game
// code that reads the wall clock breaks that: two replays of one script see
// different times and diverge. Sleeping breaks it twice over -- it also makes a
// scripted run cost real time, and is unavailable on the main thread under
// Emscripten. These tests pin both seams' contracts, including the virtual-time
// host that a replay installs, so a later change cannot quietly reintroduce a
// direct host call.

#include <cassert>
#include <cstdio>

#include "src/common/runtime.hpp"

namespace {

int64_t g_fakeNow = 0;

int64_t fakeClock() { return g_fakeNow; }

// A second source, to prove installation is not one-way.
int64_t frozenClock() { return 1234567890123LL; }

// A virtual-time host: sleeping advances the clock instead of blocking, which
// is how a replay runs a scripted play-through at full speed and still sees the
// timings the game expects.
int g_sleepCalls = 0;
int64_t g_sleptTotal = 0;

void virtualSleep(int64_t ms) {
    ++g_sleepCalls;
    g_sleptTotal += ms;
    g_fakeNow += ms;
}

}  // namespace

int main() {
    platform::PlatformContext context;
    // 1. Default is the wall clock: unset, it tracks real time and is sane.
    //    (Milliseconds since the epoch; well past 2001 and not absurd.)
    int64_t wall = context.nowMillis();
    assert(wall > 1000000000000LL);
    assert(wall == platform::wallClockMillis() || wall + 1000 > platform::wallClockMillis());

    // 2. An installed clock is authoritative -- the host, not the OS, decides.
    g_fakeNow = 5000;
    context.installClock(fakeClock);
    assert(context.nowMillis() == 5000);

    // 3. Time only moves when the host moves it. This is the property replay
    //    depends on: repeated reads within a tick see one consistent instant.
    assert(context.nowMillis() == 5000);
    assert(context.nowMillis() == 5000);

    // 4. Advancing is visible, and time can be driven in exact increments --
    //    so a replay can feed fixed dt rather than wall-clock jitter.
    g_fakeNow += 250;
    assert(context.nowMillis() == 5250);
    g_fakeNow += 250;
    assert(context.nowMillis() == 5500);

    // 5. The 250 ms cadence the canvas loop runs on reproduces exactly, with no
    //    drift after many ticks. Wall-clock timing cannot promise this.
    g_fakeNow = 0;
    for (int i = 0; i < 1000; ++i) {
        g_fakeNow += 250;
    }
    assert(context.nowMillis() == 250000);

    // 6. Installation is not one-way: a different source takes over.
    context.installClock(frozenClock);
    assert(context.nowMillis() == 1234567890123LL);

    // 7. Time may run backwards if a host replays a script from the start.
    //    Nothing in the seam prevents it; a rewind is just another install.
    g_fakeNow = 10;
    context.installClock(fakeClock);
    assert(context.nowMillis() == 10);

    // 8. nullptr restores the wall clock, so a harness can hand control back.
    context.installClock(nullptr);
    assert(context.nowMillis() > 1000000000000LL);

    // 9. Clock and sleep bindings belong to explicit contexts and do not leak.
    platform::PlatformContext first;
    platform::PlatformContext second;
    first.installClock(fakeClock);
    first.installSleep(virtualSleep);
    second.installClock(frozenClock);
    g_fakeNow = 100;
    g_sleepCalls = 0;
    g_sleptTotal = 0;
    assert(first.nowMillis() == 100);
    first.sleepFor(25);
    assert(first.nowMillis() == 125);
    assert(g_sleepCalls == 1);
    assert(g_sleptTotal == 25);
    assert(second.nowMillis() == 1234567890123LL);
    assert(first.nowMillis() == 125);

    // 10. An unbound context uses the wall-clock fallback without invoking a
    // callback installed on another context.
    second.sleepFor(0);
    assert(g_sleepCalls == 1);

    std::printf("All clock seam unit tests passed!\n");
    return 0;
}
