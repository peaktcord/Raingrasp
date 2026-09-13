#include "src/common/game/progression.hpp"

namespace progression {

void award(const Progress &p, int32_t skill, int32_t points) {
    if (skill < 0 || skill >= kSkillCount) {
        return;
    }
    SharedArray<int16_t> row = p.skills[skill];
    row[POINTS] = (int16_t)(row[POINTS] + points);
}

namespace {

void promote(const Progress &p, int32_t skill) {
    SharedArray<int16_t> row = p.skills[skill];
    row[POINTS] = (int16_t)(row[POINTS] - kPointsPerRank);
    row[RANK] = (int16_t)(row[RANK] + 1);
    int32_t bit = p.skillAttribute[skill] / 2;
    *p.levelUpMask = (int8_t)(*p.levelUpMask | (1 << bit));
    p.vitals[1] = (int16_t)(p.vitals[1] + 1);
}

bool qualifies(const Progress &p, const Rules &rules, int32_t skill) {
    int32_t points = p.skills[skill][POINTS];
    return rules.promoteAtThreshold ? points >= kPointsPerRank : points > kPointsPerRank;
}

}

bool sweep(const Progress &p, const Rules &rules, int32_t first, int32_t last) {
    for (int32_t skill = first; skill < last; ++skill) {
        if (rules.repeating) {
            while (qualifies(p, rules, skill)) {
                promote(p, skill);
            }
        } else if (qualifies(p, rules, skill)) {
            promote(p, skill);
        }
    }
    if (p.vitals[1] >= kRanksPerLevel) {
        p.vitals[0] = (int16_t)(p.vitals[0] + 1);
        return true;
    }
    return false;
}

}
