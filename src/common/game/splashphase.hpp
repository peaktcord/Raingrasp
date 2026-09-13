#ifndef COMMON_GAME_SPLASHPHASE_HPP
#define COMMON_GAME_SPLASHPHASE_HPP

#include "src/common/runtime.hpp"

namespace splashphase {

enum class Phase {
    Progress,
    CarrierLogo,
    PublisherLogo,
    Done,
};

struct State {
    Phase phase = Phase::Progress;
    int64_t phaseElapsedMs = 0;
    int64_t repaintAccumMs = 0;
    int64_t elapsedMs = 0;
};

struct Hooks {
    void (*repaint)(void *ctx, Phase phase) = nullptr;
    void (*showCarrierLogo)(void *ctx) = nullptr;
    void (*showPublisherLogo)(void *ctx) = nullptr;
    void (*finish)(void *ctx) = nullptr;
    void *ctx = nullptr;
};

bool step(State *state, const Hooks &hooks, int64_t dtMs, bool running, int32_t progressPct);

// Port addition: cut the two logo screens short.  Progress is deliberately not
// skippable -- it is the loader, not a splash, and the game is not ready until
// it reports 100%.  Returns whether the skip was acted on, so a caller can let
// the key through to whatever is on screen when the intro is already over.
bool skip(State *state, const Hooks &hooks);

}

#endif
