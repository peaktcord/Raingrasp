#ifndef STORMHOLD_DUNGEON_HPP
#define STORMHOLD_DUNGEON_HPP

#include <vector>

#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

class Monster;

class Player;

namespace stormhold {

class Dungeon : public DungeonCore {
public:
    static SharedArray<SharedArray<std::string>> dungeonNames_;
    static const int32_t kChestTier[36];
    SharedArray<int16_t> keyScratch_;
    SharedArray<int16_t> roomScratch_;
    int16_t genMinX_ = 0;
    int16_t genMaxX_ = 0;
    int16_t genMaxY_ = 0;
    int16_t genMinY_ = 0;
    std::vector<int32_t> roomKeys_;
    std::vector<int32_t> unlinkedRooms_;
    std::vector<SharedArray<int16_t>> rooms_;
    GameRandom *rng_ = nullptr;
    worldstate::DungeonRegistry &dungeons_;

    Dungeon(worldstate::DungeonRegistry &dungeons, worldstate::WorldState &worldState);
    Dungeon(worldstate::DungeonRegistry &dungeons, worldstate::WorldState &worldState,
            int8_t by, const SharedArray<int8_t> &byArray);
    Dungeon(worldstate::DungeonRegistry &dungeons, worldstate::WorldState &worldState,
            int8_t by, const SharedArray<int8_t> &byArray, int32_t n, int32_t n2,
            SharedArray<SharedArray<int8_t>> byArray2);
    void spawnRoomMonsters();
    void placeMonster(Monster *d2, SharedArray<int16_t> sArray);
    void spawnMonsters(int32_t n) override;
    void spawnNear(Player *j2);
    static int32_t compareInt(int32_t n, int32_t n2, bool bl);
    SharedArray<int32_t> pickRooms(int32_t n);
    void placeChests();
    void populate();
    void rebuildOccupancy();
    Monster *monsterAt(Monster *target, int32_t n, int32_t n2) override;
    void removeDroppedItem(const SharedArray<int8_t> &byArray);
    int32_t countDroppedItems(int32_t n, int32_t n2);
    SharedArray<int8_t> firstDroppedItem(int32_t n, int32_t n2);
    void b(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray);
    void scanSurroundings(int32_t x, int32_t y, int32_t facing,
                          SharedArray<SharedArray<int8_t>> into) override;
    void c(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray);
    void a(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray);
    void a(int32_t n, int32_t n2, int32_t n3, int32_t n4, SharedArray<SharedArray<int8_t>> byArray);
    SharedArray<int32_t> screenSlotOf(int32_t n, int32_t n2, int32_t n3, int32_t n4, int32_t n5);
    int8_t tileAt(int32_t n, int32_t n2);
    int8_t a(int32_t n, int32_t n2, SharedArray<SharedArray<int8_t>> byArray);
    SharedArray<std::string> name();
    static void loadNames(platform::PlatformContext *context);
    void refreshMonsterBits(int32_t n, int32_t n2);
    static void initializeStatics();
};

inline Dungeon *dungeonOf(Player &player) { return static_cast<Dungeon *>(player.dungeon()); }
inline Dungeon *dungeonOf(Player *player) { return dungeonOf(*player); }

}
#endif
