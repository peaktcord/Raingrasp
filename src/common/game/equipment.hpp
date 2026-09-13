#ifndef COMMON_GAME_EQUIPMENT_HPP
#define COMMON_GAME_EQUIPMENT_HPP

#include "src/common/runtime.hpp"

namespace equipment {

enum Slot {
    WEAPON = 0,
    CUIRASS = 1,
    BOOTS = 2,
    GLOVES = 3,
    HELMET = 4,
    SHIELD = 5,
    LOCKPICK = 6,
};

const int32_t kReadableSlotCount = 6;

const int32_t kStoredSlotCount = 7;

const int32_t kNotEquippable = -1;

}

#endif
