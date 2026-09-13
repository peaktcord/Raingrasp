#ifndef COMMON_GAME_SKILLS_HPP
#define COMMON_GAME_SKILLS_HPP

#include "src/common/runtime.hpp"

namespace skills {

enum Skill {
    AXE = 0,
    ALTERATION = 1,
    BLUNT = 2,
    CONJURATION = 3,
    DESTRUCTION = 4,
    HEAVY_ARMOR = 5,
    ILLUSION = 6,
    LIGHT_ARMOR = 7,
    LONG_BLADE = 8,
    PERCEPTION = 9,
    RESTORATION = 10,
    SECURITY = 11,
    SHORT_BLADE = 12,
    SPEECHCRAFT = 13,
};

const int32_t kSkillCount = 14;

}

#endif
