#include "src/common/replay/replay.hpp"

#include <cstdio>

namespace replay {
namespace {

int64_t g_now = 0;

int64_t replayClock() { return g_now; }

void replaySleep(int64_t ms) {
    if (ms > 0) g_now += ms;
}

}

std::vector<TickResult> run(Probe &probe, const Script &script,
                            const std::string &resourceDir, const std::string &saveDir,
                            bool *bootOk) {
    std::vector<TickResult> rows;

    g_now = script.seed;
    platform::PlatformContext *context = probe.platformContext();
    context->installClock(replayClock);
    context->installSleep(replaySleep);

    bool ok = probe.boot(resourceDir, saveDir);
    if (bootOk != nullptr) *bootOk = ok;
    if (!ok) {
        context->installClock(nullptr);
        context->installSleep(nullptr);
        return rows;
    }

    size_t nextEvent = 0;
    for (int tick = 0; tick < script.ticks; ++tick) {
        while (nextEvent < script.events.size() &&
               script.events[nextEvent].tick == tick) {
            const Event &event = script.events[nextEvent];
            probe.key(event.key, event.press);
            ++nextEvent;
        }

        g_now += script.tickMs;
        probe.step(script.tickMs);

        TickResult row;
        row.tick = tick;
        Hasher hasher;
        probe.hashState(hasher);
        row.hash = hasher.value();
        row.note = probe.describe();
        rows.push_back(row);
    }

    context->installClock(nullptr);
    context->installSleep(nullptr);
    return rows;
}

std::string toTsv(const char *scriptName, const std::vector<TickResult> &rows) {
    std::string out;
    char buffer[128];
    for (const TickResult &row : rows) {
        std::snprintf(buffer, sizeof(buffer), "%s\t%d\t%016llx\t", scriptName, row.tick,
                      (unsigned long long)row.hash);
        out += buffer;
        out += row.note;
        out += "\n";
    }
    return out;
}

}
