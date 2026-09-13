#ifndef COMMON_GAME_MONSTER_HPP
#define COMMON_GAME_MONSTER_HPP

#include "src/common/game/combatant.hpp"
#include "src/common/game/dungeon_core.hpp"
#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

namespace platform { class PlatformContext; }

class Monster {
public:
    static int32_t typeCount_;
    static SharedArray<std::string> typeNames_;
    static SharedArray<SharedArray<int8_t>> typeStats_;
    int16_t uid_ = 0;
    int8_t type_ = 0;
    int8_t hp_ = 0;
    int8_t gridX_ = 0;
    int8_t gridY_ = 0;
    bool seen_ = false;
    int8_t dungeonId_ = 0;
    SharedArray<int8_t> effects_;
    int8_t moveCounter_ = 0;
    int8_t attackPhase_ = 0;
    int64_t lastActionMs_ = 0;
    DungeonCore *dungeon_ = nullptr;

    Monster();
    Monster(int32_t uid, int32_t type, int32_t dungeonId);

    SharedArray<int8_t> toRecord();
    static Monster *fromRecord(Monster *into, const SharedArray<int8_t> &record,
                               DungeonCore *dungeon = nullptr);
    void store();

    std::string name();
    int32_t stat(int32_t n);
    bool isUndead();
    void takeDamage(int32_t n);

    bool stepDir(int32_t n);
    bool takeTurn(int32_t x, int32_t y);
    void takeTurn(Combatant *target);
    void pursue(int32_t x, int32_t y);
    bool isEntranceTile(int32_t n, int32_t n2);
    bool isInRange(Combatant *target);
    int32_t distanceTo(Combatant *target);
    bool isAdjacent(Combatant *target);
    bool attack(Combatant *target, int64_t nowMs);

    static void loadTypes(platform::PlatformContext *context);
    static Monster *readFrom(BinaryReader *dataInputStream);
    void writeTo(BinaryWriter *dataOutputStream);
    void dropLoot(bool always);

    static Monster *spawn(GameRandom *random, int32_t level, DungeonCore *dungeon, int32_t type);
    static Monster *spawn(GameRandom *random, DungeonCore *dungeon, int32_t type);
    static Monster *spawnDefault(DungeonCore *dungeon);
};

#endif
