#ifndef COMMON_GAME_VITALS_HPP
#define COMMON_GAME_VITALS_HPP

#include "src/common/runtime.hpp"

#include "src/common/game/skills.hpp"
#include "src/common/game/spells.hpp"

namespace vitals {

enum Vital {
    LEVEL = 0,
    EXPERIENCE = 1,
    CUR_HP = 2,
    MAX_HP = 3,
    CUR_MAGICKA = 4,
    MAX_MAGICKA = 5,
    CUR_FATIGUE = 6,
    MAX_FATIGUE = 7,
    CORRUPTION = 8,
    CORRUPTION_PROGRESS = 9,
    kVitalCount = 10
};

const int32_t kSpellCount = spellid::kSpellCount;

const int32_t kFortifyVitalsSpell = spellid::RAISE_ATTRIBUTE;

const int32_t kRestorationSkill = skills::RESTORATION;

enum Sentinel {
    INACTIVE = 0,
    PERMANENT = -1,
    WHILE_TARGETED = -2,
    UNDECODED = -4,
};

const int32_t kFatigueAilmentMask = 1;

bool spellActive(const SharedArray<int8_t> &timers, int32_t spell, bool hasTarget);

void clearSpell(SharedArray<int8_t> &timers, int32_t spell);

int32_t spellSchool(int32_t spell);

int32_t effectiveVital(const SharedArray<int16_t> &vitals, int32_t vital, bool fortifyActive,
                    int32_t restorationRank);

int32_t fatigueMultiplier(int32_t ailments);

}

#endif
