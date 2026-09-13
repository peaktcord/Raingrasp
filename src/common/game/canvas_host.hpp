#ifndef COMMON_GAME_CANVAS_HOST_HPP
#define COMMON_GAME_CANVAS_HOST_HPP

#include "src/common/game/world_state.hpp"
#include "src/common/game/display_target.hpp"

class GameCanvas;

namespace platform { class PlatformContext; }

namespace game {

struct Profile;

class CanvasHost {
public:
    virtual ~CanvasHost() = default;

    virtual const Profile &profile() const = 0;
    virtual worldstate::WorldState &worldState() = 0;
    virtual platform::PlatformContext *platformContext() = 0;

    virtual void startCanvasLoop(GameCanvas *canvas) = 0;

    virtual void showDisplayable(DisplayTarget target) = 0;
    virtual void showLevelUp() = 0;
    virtual void showEndOfGame() = 0;
    virtual void showOptions() = 0;
};

}

#endif
