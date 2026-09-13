#ifndef DAWNSTAR_DUNGEON_GEN_HPP
#define DAWNSTAR_DUNGEON_GEN_HPP

#include <vector>

#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

class UIWidget;

namespace dawnstar {

class Dungeon;

class DungeonGen {
public:
    SharedArray<SharedArray<int8_t>> geometry_;
    static const int32_t kChestTier[36];
    std::vector<int32_t> roomKeys_;
    std::vector<int32_t> unlinkedRooms_;
    std::vector<SharedArray<int16_t>> rooms_;
    GameRandom *rng_ = nullptr;
    int32_t campWidth_ = 0;
    int32_t campHeight_ = 0;
    SharedArray<SharedArray<int8_t>> campGrid_;
    platform::PlatformContext *platformContext_ = nullptr;

    DungeonGen() = default;
    DungeonGen(worldstate::DungeonRegistry &dungeons, UIWidget *h2,
               worldstate::WorldState &worldState);
    void buildCampGrid();
    void loadGeometry();
    void generate(Dungeon *i2);
    void placeChests(SharedArray<int32_t> nArray, Dungeon *i2);
    SharedArray<int16_t> rollRoom();
    bool tryPlaceRoom(SharedArray<int16_t> sArray, Dungeon *i2);
    void linkRooms(Dungeon *i2);
    int32_t carveEntrance(Dungeon *i2, int16_t s);
    void carveCorridor(Dungeon *i2, int32_t n, int32_t n2);
    void carveRect(Dungeon *i2, int32_t n, int32_t n2, int32_t n3, int32_t n4);
    int32_t distanceSquared(int32_t n, int32_t n2);
    int32_t packKey(int16_t s, int16_t s2);
    SharedArray<int16_t> unpackKey(int32_t n);
    void addRoomKey(int32_t n);
    bool isExitDirection(int16_t s);
};

}
#endif
