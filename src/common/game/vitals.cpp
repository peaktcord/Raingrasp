#include "src/common/game/vitals.hpp"

namespace vitals {

bool spellActive(const SharedArray<int8_t> &timers, int32_t spell, bool hasTarget) {
    int8_t timer = timers[spell - 1];
    if (timer == PERMANENT) return true;
    if (timer == WHILE_TARGETED) return hasTarget;
    return timer > 0;
}

void clearSpell(SharedArray<int8_t> &timers, int32_t spell) { timers[spell - 1] = INACTIVE; }

int32_t spellSchool(int32_t spell) {
    if (spell <= 5) return skills::ALTERATION;
    if (spell <= 10) return skills::CONJURATION;
    if (spell <= 15) return skills::DESTRUCTION;
    if (spell <= 20) return skills::ILLUSION;
    return skills::RESTORATION;
}

int32_t effectiveVital(const SharedArray<int16_t> &vitals, int32_t vital, bool fortifyActive,
                    int32_t restorationRank) {
    int32_t value = vitals[vital];
    if (!fortifyActive) return value;

    int32_t maximum;
    switch (vital) {
        case CUR_HP: maximum = vitals[MAX_HP]; break;
        case CUR_MAGICKA: maximum = vitals[MAX_MAGICKA]; break;
        case CUR_FATIGUE: maximum = vitals[MAX_FATIGUE]; break;
        default: return value;
    }

    value += restorationRank;
    return value > maximum ? maximum : value;
}

int32_t fatigueMultiplier(int32_t ailments) {
    return (ailments & kFatigueAilmentMask) == kFatigueAilmentMask ? 3 : 1;
}

}
