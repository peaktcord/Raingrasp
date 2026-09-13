#ifndef DAWNSTAR_DUNGEON_HPP
#define DAWNSTAR_DUNGEON_HPP

#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

class Monster;

class Player;

namespace dawnstar {

class Dungeon : public DungeonCore {
public:
    static SharedArray<std::string> dungeonNames_;
    worldstate::DungeonRegistry &dungeons_;

    Dungeon(worldstate::DungeonRegistry &dungeons, worldstate::WorldState &worldState);
    Dungeon(worldstate::DungeonRegistry &dungeons, worldstate::WorldState &worldState,
            int8_t by, SharedArray<int8_t> byArray);
    Dungeon(worldstate::DungeonRegistry &dungeons, worldstate::WorldState &worldState,
            int8_t by, SharedArray<int8_t> byArray, int32_t n, int32_t n2,
            SharedArray<SharedArray<int8_t>> byArray2);
    void spawnMonsters(int32_t n) override;
    bool spawnNear(int32_t n, int32_t n2, int32_t n3);
    void rebuildOccupancy();
    int8_t runMonsters(int64_t l, Player *j2);
    void removeDroppedItem(SharedArray<int8_t> byArray);
    void clearItemBit(int32_t n, int32_t n2);
    void a(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray);
    void a(int32_t n, int32_t n2, int32_t n3, int32_t n4, SharedArray<SharedArray<int8_t>> byArray);
    void scanSurroundings(int32_t x, int32_t y, int32_t facing,
                          SharedArray<SharedArray<int8_t>> into) override;
    Monster *monsterAt(Monster *into, int32_t x, int32_t y) override;
    int8_t tileAt(int32_t n, int32_t n2);
    std::string name();
    static void initializeStatics();
};

inline Dungeon *dungeonOf(Player &player) { return static_cast<Dungeon *>(player.dungeon()); }
inline Dungeon *dungeonOf(Player *player) { return dungeonOf(*player); }

}
#endif
