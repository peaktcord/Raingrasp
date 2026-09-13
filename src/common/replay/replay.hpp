#ifndef COMMON_REPLAY_HPP
#define COMMON_REPLAY_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "src/common/runtime.hpp"

namespace replay {

class Hasher {
    uint64_t h_ = 1469598103934665603ULL;
public:
    void byte(uint8_t v) {
        h_ ^= v;
        h_ *= 1099511628211ULL;
    }
    void i64(int64_t v) {
        for (int n1 = 0; n1 < 8; ++n1) byte((uint8_t)((uint64_t)v >> (n1 * 8)));
    }
    void i32(int32_t v) { i64((int64_t)v); }
    void str(const std::string &s) {
        for (char c : s) byte((uint8_t)c);
        byte(0);
    }
    uint64_t value() const { return h_; }
};

struct Event {
    int tick;
    int32_t key;
    bool press;
};

struct Script {
    const char *name = "";
    int64_t seed = 0;
    int ticks = 0;
    int64_t tickMs = 250;
    std::vector<Event> events;
};

struct Probe {
    virtual ~Probe() {}
    virtual platform::PlatformContext *platformContext() = 0;
    virtual bool boot(const std::string &resourceDir, const std::string &saveDir) = 0;
    virtual void key(int32_t code, bool press) = 0;
    virtual void step(int64_t dtMs) = 0;
    virtual void hashState(Hasher &out) = 0;
    virtual std::string describe() = 0;
};

struct TickResult {
    int tick = 0;
    uint64_t hash = 0;
    std::string note;
};

std::vector<TickResult> run(Probe &probe, const Script &script,
                            const std::string &resourceDir, const std::string &saveDir,
                            bool *bootOk);

std::string toTsv(const char *scriptName, const std::vector<TickResult> &rows);

}

#endif
