#include "src/common/game/splashphase.hpp"

namespace splashphase {

bool step(State *state, const Hooks &hooks, int64_t dtMs, bool running, int32_t progressPct) {
    state->phaseElapsedMs += dtMs;
    state->repaintAccumMs += dtMs;
    bool repaintDue = state->repaintAccumMs >= 500;
    if (repaintDue) {
        state->repaintAccumMs -= 500;
    }

    switch (state->phase) {
        case Phase::Progress: {
            state->elapsedMs = state->phaseElapsedMs;
            if (running && (progressPct < 100 || state->elapsedMs < 4000)) {
                if (repaintDue) {
                    hooks.repaint(hooks.ctx, Phase::Progress);
                }
                return true;
            }
            hooks.showCarrierLogo(hooks.ctx);
            state->phase = Phase::CarrierLogo;
            state->phaseElapsedMs = 0;
            return true;
        }
        case Phase::CarrierLogo: {
            if (repaintDue) {
                hooks.repaint(hooks.ctx, Phase::CarrierLogo);
            }
            if (state->phaseElapsedMs <= 2000) {
                return true;
            }
            hooks.showPublisherLogo(hooks.ctx);
            state->phase = Phase::PublisherLogo;
            state->phaseElapsedMs = 0;
            return true;
        }
        case Phase::PublisherLogo: {
            if (repaintDue) {
                hooks.repaint(hooks.ctx, Phase::PublisherLogo);
            }
            if (state->phaseElapsedMs <= 1000) {
                return true;
            }
            state->phase = Phase::Done;
            hooks.finish(hooks.ctx);
            return false;
        }
        case Phase::Done:
        default:
            return false;
    }
}

bool skip(State *state, const Hooks &hooks) {
    // Only the two logo phases can be cut short.  The loader owns Progress --
    // jumping out of it would show the menu over half-built state -- so a press
    // there is dropped, not queued.  Done means the intro is long gone and the
    // key belongs to whatever is on screen now.
    if (state->phase != Phase::CarrierLogo && state->phase != Phase::PublisherLogo) {
        return false;
    }
    state->phase = Phase::Done;
    state->phaseElapsedMs = 0;
    state->repaintAccumMs = 0;
    hooks.finish(hooks.ctx);
    return true;
}

}
