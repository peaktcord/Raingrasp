#ifndef COMMON_HOST_GAME_HOST_HPP
#define COMMON_HOST_GAME_HOST_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "src/common/game/menuaction.hpp"
#include "src/common/input/menu_shortcuts.hpp"
#include "src/common/ui.hpp"
#include "src/common/replay/replay.hpp"

namespace platform { class PlatformContext; }
namespace render {
class Context;
class Surface;
}

namespace host {

enum class TickStatus { Idle, Ran, Paused, Stopped, Failed };

class GameHost {
public:
    virtual ~GameHost() = default;

    virtual const char *id() const = 0;
    virtual const char *title() const = 0;
    virtual const char *defaultSaveDir() const = 0;
    virtual const menuaction::Action *optionsRows(int32_t *count) const = 0;

    virtual void boot(platform::PlatformContext *context, render::Context *renderer,
                      render::Surface *screen) = 0;

    virtual Display *display() = 0;

    virtual bool hasSplash() = 0;
    virtual bool stepSplash(int64_t ms) = 0;
    // Cuts the logo screens short.  No effect while the loader is still working
    // or once the intro is over; returns whether the skip was acted on, so the
    // caller knows whether the key was consumed.
    virtual bool skipSplash() = 0;

    virtual void drainPendingWork() {}

    virtual bool hasCanvas() = 0;
    virtual bool canvasRunning() = 0;
    virtual TickStatus tick(int64_t *periodMs) = 0;

    virtual void requestExit() = 0;

    virtual bool switchRequested() const = 0;

    virtual void keyPressed(int32_t code) = 0;
    virtual void keyReleased(int32_t code) = 0;

    virtual void repaintCanvas() = 0;
    virtual void repaintCanvasNow() = 0;
    virtual void refreshPortOptionsUI() = 0;

    virtual const shortcuts::Host *shortcutHost() = 0;

    virtual void setWideView(bool on) = 0;
    virtual bool wideView() const = 0;

    virtual replay::Script replayScript() = 0;
    virtual void hashState(replay::Hasher &out) = 0;
    virtual std::string describe() = 0;
    virtual std::string bootSummary() = 0;
};

void pump(GameHost *game, int64_t nowMs, int64_t *nextTickMs, int64_t *nextSplashMs);

bool completeNameForm(Display *display, const char *name);

}

#endif
