#ifndef COMMON_GAME_MONSTERDATA_HPP
#define COMMON_GAME_MONSTERDATA_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

namespace monsterdata {

const int32_t kEffectCount = 10;

struct TypeTable {
    int32_t count = 0;
    SharedArray<std::string> names;
    SharedArray<SharedArray<int8_t>> stats;
};

std::string typeName(const TypeTable &table, int32_t type);

int32_t typeStat(const TypeTable &table, int32_t type, int32_t column);

bool isUndeadType(int32_t type);

int32_t applyDamage(int32_t hp, int32_t damage);

struct Fields {
    int16_t uid = 0;
    int8_t type = 0;
    int8_t hp = 0;
    int8_t gridX = 0;
    int8_t gridY = 0;
    bool seen = false;
    int8_t dungeonId = 0;
    int8_t moveCounter = 0;
    int8_t attackPhase = 0;
    int64_t lastActionMs = 0;
    SharedArray<int8_t> effects;

    Fields() : effects(kEffectCount) {}
};

void readFields(BinaryReader *in, Fields *out);
void writeFields(BinaryWriter *out, const Fields &fields);

}

#endif
