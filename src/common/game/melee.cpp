#include "src/common/game/melee.hpp"

#include "src/common/game/formulas.hpp"
#include "src/common/game/spells.hpp"

namespace melee {

namespace {

enum Stat {
    EVASION = 2,
    DEFENCE = 6,
    DODGE = 7,
    ARMOUR = 8,
    MAX_HP = 14,
};

enum {
    SPELL_DOT = spellid::BLOOD_SPIRIT,
    SPELL_TO_HIT = spellid::RIGHTEOUSNESS,
    SPELL_ARMOUR = spellid::HARM_ARMOR,
    EFFECT_DOT = 1,
    EFFECT_ARMOUR = 5,
    EFFECT_TO_HIT = 8,
    TIMER_ARMOUR = spellid::HARM_ARMOR - 1,
};

const int32_t AILMENT_BLEED = 6;

enum { HEALTH = 2, HEALTH_MAX = 3, FATIGUE = 6 };

void swingBody(const Attacker &a, const Target &t, bool useRolls, int32_t attackRoll,
               int32_t defenceRoll) {
    a.setTargetUid(a.self, t.uid);
    int8_t unusedType = t.type;
    (void)unusedType;

    int32_t advantage = a.attackSkill(a.self, true) - t.statOf(DODGE);
    advantage = min32(advantage, t.statOf(EVASION));
    if (a.spellActive(a.self, SPELL_TO_HIT)) {
        if (t.effectAt(EFFECT_TO_HIT) == 0) {
            a.clearSpell(a.self, SPELL_TO_HIT);
        } else {
            advantage += t.effectAt(EFFECT_TO_HIT);
        }
    }
    int32_t defence = GameFormulas::clampChance(t.statOf(DEFENCE) - advantage * 5);
    int32_t attack = GameFormulas::clampChance(a.attackAptitude(a.self) + advantage * 5);
    int32_t roll = useRolls
                    ? GameFormulas::resolveCombatRoll(attack, defence, attackRoll, defenceRoll,
                                                      nullptr)
                    : GameFormulas::resolveCombatRoll(a.random, attack, defence);
    if (roll == 0) {
        return;
    }

    int32_t damage = a.weaponDamage(a.self);
    int32_t armour = t.statOf(ARMOUR);
    if (a.spellActive(a.self, SPELL_ARMOUR)) {
        if (t.effectAt(EFFECT_ARMOUR) == 0) {
            a.clearSpellTimerRaw(a.self, TIMER_ARMOUR);
        } else {
            armour -= t.effectAt(EFFECT_ARMOUR);
        }
    }
    if (roll == 1) {
        armour = 2 * armour;
    } else if (roll == 3) {
        damage = 2 * damage;
    }
    int32_t dealt = GameFormulas::calcDamage(damage, armour, t.statOf(MAX_HP));
    t.damage(dealt);
    t.save();

    if (a.spellActive(a.self, SPELL_DOT)) {
        if (t.effectAt(EFFECT_DOT) == 0) {
            a.clearSpell(a.self, SPELL_DOT);
        } else {
            dealt = GameFormulas::calcDamage(t.effectAt(EFFECT_DOT), 0, t.statOf(MAX_HP));
            t.damage(dealt);
        }
    }

    if (roll >= 2) {
        a.awardSkillXp(a.self, a.attackSkillIndex(a.self), 1);
    }
    if (!a.spellActive(a.self, SPELL_DOT)) {
        a.vitals[FATIGUE] =
            (int16_t)(a.vitals[FATIGUE] -
                     GameFormulas::calcSwingFatigueCost(a.fatigueMultiplier(a.self)));
        a.vitals[FATIGUE] = (int16_t)max32(a.vitals[FATIGUE], 0);
    }
    if (a.hasAilment(a.self, AILMENT_BLEED)) {
        int32_t bleed = 2 * a.vitals[HEALTH_MAX] / 100;
        if (bleed < 1) {
            bleed = 1;
        }
        a.vitals[HEALTH] = (int16_t)(a.vitals[HEALTH] - (int16_t)bleed);
    }
}

}

void swing(const Attacker &a, const Target &t) { swingBody(a, t, false, 0, 0); }

void swingWithRolls(const Attacker &a, const Target &t, int32_t attackRoll, int32_t defenceRoll) {
    swingBody(a, t, true, attackRoll, defenceRoll);
}

}
