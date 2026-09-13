#ifndef COMMON_GAME_WORLD_HPP
#define COMMON_GAME_WORLD_HPP

#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

class Monster;

namespace platform { class PlatformContext; }

namespace game {

struct Profile;

class World {
public:
    virtual ~World() = default;

    virtual const Profile &profile() const = 0;

    virtual worldstate::WorldState &worldState() = 0;
    virtual platform::PlatformContext *platformContext() = 0;

    virtual DungeonCore *dungeonAt(int32_t dungeonId) = 0;

    virtual void removeMonsterAt(int32_t dungeonId, int32_t x, int32_t y) = 0;

    virtual int32_t gameAdvancementLevel(int32_t giftPoints) = 0;
    virtual void openAndRepopulateDungeons(int32_t level) = 0;

    virtual void refreshAhead() = 0;

    virtual Monster *combatTarget() = 0;
};

}

#endif
