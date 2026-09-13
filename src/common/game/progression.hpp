#ifndef COMMON_GAME_PROGRESSION_HPP
#define COMMON_GAME_PROGRESSION_HPP

#include "src/common/runtime.hpp"

namespace progression {

constexpr int32_t kSkillCount = 14;

constexpr int32_t kPointsPerRank = 10;
constexpr int32_t kRanksPerLevel = 10;

enum Column {
    RANK = 0,
    APTITUDE = 1,
    POINTS = 2,
};

struct Rules {
    bool promoteAtThreshold = false;

    bool repeating = false;
};

struct Progress {
    SharedArray<SharedArray<int16_t>> skills;

    SharedArray<int16_t> skillAttribute;

    SharedArray<int16_t> vitals;

    int8_t *levelUpMask = nullptr;
};

void award(const Progress &p, int32_t skill, int32_t points);

bool sweep(const Progress &p, const Rules &rules, int32_t first, int32_t last);

inline bool sweepAll(const Progress &p, const Rules &rules) {
    return sweep(p, rules, 0, kSkillCount);
}

}

#endif
