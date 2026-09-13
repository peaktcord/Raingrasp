// The replay driver, against a synthetic probe.
//
// The real probes cannot exist until the loop inversion gives the game a
// step(), but the driver's own guarantees can be pinned now, and they are what
// every later baseline rests on: the same script produces the same hashes, a
// different script does not, time is script-derived rather than wall-clock, and
// running two scripts in one process matches running them separately.
//
// The fake probe below is deliberately sensitive to everything a replay is
// supposed to control -- it folds the clock, the events it receives, and its own
// accumulated state into its hash -- so any of those going non-deterministic
// shows up as a hash mismatch here rather than in a game baseline nobody can
// debug.

#include <cassert>
#include <cstdio>
#include <string>

#include "src/common/replay/replay.hpp"

namespace {

struct FakeProbe : replay::Probe {
    platform::PlatformContext context;
    int steps = 0;
    int keysSeen = 0;
    int64_t lastClock = 0;
    int64_t accumulator = 0;
    bool bootCalled = false;
    bool failBoot = false;

    platform::PlatformContext *platformContext() override { return &context; }

    bool boot(const std::string &, const std::string &) override {
        bootCalled = true;
        // Boot reads the clock, the way Game's static init seeds its RNG.
        lastClock = context.nowMillis();
        accumulator = (int64_t)lastClock;
        return !failBoot;
    }

    void key(int32_t code, bool press) override {
        ++keysSeen;
        accumulator = accumulator * 31 + code + (press ? 1 : 0);
    }

    void step(int64_t dtMs) override {
        ++steps;
        // A sleep inside the tick, as the real loops do. Under replay this
        // advances the virtual clock instead of blocking.
        context.sleepFor(10);
        lastClock = context.nowMillis();
        accumulator = accumulator * 31 + dtMs + lastClock;
    }

    void hashState(replay::Hasher &out) override {
        out.i32(steps);
        out.i32(keysSeen);
        out.i64(accumulator);
        out.i64(lastClock);
    }

    std::string describe() override {
        return "steps=" + std::to_string(steps) + " keys=" + std::to_string(keysSeen);
    }
};

replay::Script basicScript() {
    replay::Script script;
    script.name = "basic";
    script.seed = 1000000;
    script.ticks = 20;
    script.tickMs = 250;
    script.events = {
        {3, -1, true},   // up, pressed at tick 3
        {3, -1, false},
        {7, 53, true},   // '5'
        {7, 53, false},
        {15, -7, true},  // soft right
    };
    return script;
}

std::vector<uint64_t> hashes(const std::vector<replay::TickResult> &rows) {
    std::vector<uint64_t> out;
    for (const replay::TickResult &row : rows) out.push_back(row.hash);
    return out;
}

}  // namespace

int main() {
    const replay::Script script = basicScript();

    // 1. A run produces one row per tick, in order.
    FakeProbe probe1;
    bool ok1 = false;
    std::vector<replay::TickResult> run1 = replay::run(probe1, script, "res", "save", &ok1);
    assert(ok1);
    assert(probe1.bootCalled);
    assert(run1.size() == 20);
    for (int n1 = 0; n1 < 20; ++n1) assert(run1[(size_t)n1].tick == n1);
    assert(probe1.steps == 20);

    // 2. Every event was delivered, exactly once.
    assert(probe1.keysSeen == 5);

    // 3. The same script replays identically. This is the whole point: it is
    //    what lets a baseline detect that a refactor changed behaviour.
    FakeProbe probe2;
    std::vector<replay::TickResult> run2 = replay::run(probe2, script, "res", "save", nullptr);
    assert(hashes(run1) == hashes(run2));

    // 4. Two runs in one process match two separate runs -- the driver restores
    //    the seams, so the second is not polluted by the first.
    FakeProbe probe3;
    std::vector<replay::TickResult> run3 = replay::run(probe3, script, "res", "save", nullptr);
    assert(hashes(run1) == hashes(run3));

    // 5. Time is script-derived, not wall-clock. Boot sees exactly the seed,
    //    and time advances *before* each tick, so a tick that reads the clock
    //    sees this tick's instant rather than the previous one's. The game
    //    depends on that: GameCanvas::tick() recomputes its timestamps from
    //    the clock, and they reach Monster::attack's `l - lastActionMs_ > 800`.
    FakeProbe probe4;
    replay::run(probe4, script, "res", "save", nullptr);
    // 20 ticks x (250 ms advance + 10 ms slept inside the tick).
    assert(probe4.lastClock == script.seed + 20 * (250 + 10));

    // 6. A different seed gives different hashes. A replay that ignored its
    //    seed would pass test 3 and still be worthless.
    replay::Script other = basicScript();
    other.seed = 2000000;
    FakeProbe probe5;
    std::vector<replay::TickResult> run5 = replay::run(probe5, other, "res", "save", nullptr);
    assert(hashes(run5) != hashes(run1));

    // 7. Different input gives different hashes, from the first affected tick
    //    onward -- and identical hashes before it, since nothing had diverged.
    replay::Script moved = basicScript();
    moved.events[0].tick = 5;
    FakeProbe probe6;
    std::vector<replay::TickResult> run6 = replay::run(probe6, moved, "res", "save", nullptr);
    assert(run6[2].hash == run1[2].hash);   // before either press
    assert(run6[3].hash != run1[3].hash);   // one script pressed here
    assert(hashes(run6) != hashes(run1));

    // 8. The seams are released afterwards, so unrelated code is unaffected.
    assert(platform::wallClockMillis() > 1000000000000LL);

    // 9. A failed boot reports itself and runs nothing, rather than producing a
    //    baseline full of hashes of an empty game.
    FakeProbe probe7;
    probe7.failBoot = true;
    bool ok7 = true;
    std::vector<replay::TickResult> run7 = replay::run(probe7, script, "res", "save", &ok7);
    assert(!ok7);
    assert(run7.empty());
    assert(probe7.steps == 0);
    assert(platform::wallClockMillis() > 1000000000000LL);  // still released

    // 10. TSV carries the script name, tick, a fixed-width hash and the note.
    std::string tsv = replay::toTsv("basic", run1);
    assert(tsv.find("basic\t0\t") == 0);
    assert(tsv.find("steps=20 keys=5") != std::string::npos);
    // One line per tick.
    size_t lines = 0;
    for (char c : tsv) if (c == '\n') ++lines;
    assert(lines == 20);

    std::printf("All replay driver unit tests passed!\n");
    return 0;
}
