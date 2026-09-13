#ifndef STORMHOLD_DUNGEON_GEN_HPP
#define STORMHOLD_DUNGEON_GEN_HPP

#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

namespace stormhold {

class Dungeon;

class DungeonGen {
public:
    int32_t campWidth_ = 0;
    int32_t campHeight_ = 0;
    SharedArray<SharedArray<int8_t>> campGrid_;

    DungeonGen() = default;

    void buildCampGrid();

    void generate(Dungeon *i2);
    void generateLayout(Dungeon *i2, int64_t l, int16_t s, int16_t s2);
    SharedArray<int16_t> rollRoom(Dungeon *i2);
    bool tryPlaceRoom(Dungeon *i2, SharedArray<int16_t> sArray);
    void linkRooms(Dungeon *i2);
    int32_t carveEntrance(Dungeon *i2, int16_t s);
    void carveCorridor(Dungeon *i2, int32_t n, int32_t n2);
    void carveRect(Dungeon *i2, int32_t n, int32_t n2, int32_t n3, int32_t n4);
    int32_t packKey(int16_t s, int16_t s2);
    SharedArray<int16_t> unpackKey(Dungeon *i2, int32_t n);
    void addRoomKey(Dungeon *i2, int32_t n);
    int32_t distanceSquared(Dungeon *i2, int32_t n, int32_t n2);
    static bool isExitDirection(int16_t s);
};

}
#endif
