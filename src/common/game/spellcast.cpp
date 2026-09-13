#include "src/common/game/spellcast.hpp"

#include "src/common/game/formulas.hpp"
#include "src/common/game/skills.hpp"
#include "src/common/game/spells.hpp"
#include "src/common/game/vitals.hpp"

namespace spellcast {

using vitals::CUR_FATIGUE;
using vitals::CUR_HP;
using vitals::CUR_MAGICKA;
using vitals::MAX_FATIGUE;
using vitals::MAX_HP;
using vitals::MAX_MAGICKA;

namespace {

int32_t clampChance(int32_t chance) {
    return min32(max32(chance, kChanceFloor), kChanceCeiling);
}

Outcome finish(const Caster &caster, int32_t skill, int32_t roll, int32_t magickaCost) {
    Outcome out;
    out.roll = roll;
    out.multiplier = 1;

    SharedArray<int16_t> vitals = caster.vitals;
    if (out.roll == 0) {
        vitals[CUR_MAGICKA] = (int16_t)(vitals[CUR_MAGICKA] - 3 * magickaCost);
    } else if (out.roll == 1) {
        vitals[CUR_MAGICKA] = (int16_t)(vitals[CUR_MAGICKA] - 3 * magickaCost / 2);
    } else if (out.roll == 2) {
        vitals[CUR_MAGICKA] = (int16_t)(vitals[CUR_MAGICKA] - magickaCost);
    } else if (out.roll == 3) {
        vitals[CUR_MAGICKA] = (int16_t)(vitals[CUR_MAGICKA] - magickaCost);
        out.multiplier = 2;
    }
    vitals[CUR_MAGICKA] = (int16_t)max32(vitals[CUR_MAGICKA], 0);

    if (out.roll >= 2) {
        caster.awardSkillXp(caster.self, skill, 1);
    }
    return out;
}

}

Outcome resolve(const Caster &caster, int32_t skill, int32_t advantage, int32_t difficulty,
                int32_t magickaCost) {
    int32_t roll = GameFormulas::resolveCombatRoll(caster.random, clampChance(advantage),
                                                clampChance(difficulty));
    return finish(caster, skill, roll, magickaCost);
}

Outcome resolveWithRolls(const Caster &caster, int32_t skill, int32_t advantage, int32_t difficulty,
                         int32_t magickaCost, int32_t attackRoll, int32_t defenceRoll) {
    int32_t roll = GameFormulas::resolveCombatRoll(clampChance(advantage), clampChance(difficulty),
                                                attackRoll, defenceRoll, nullptr);
    return finish(caster, skill, roll, magickaCost);
}

void settle(const Caster &caster);

namespace {

void castAtMonsterImpl(const Caster &caster, const Target &target,
                       const MonsterSpellHooks &hooks, int32_t spellId, int32_t school,
                       int32_t magickaCost, bool drawn, int32_t attackRoll, int32_t defenceRoll) {
    void *self = caster.self;

    int32_t resist = target.stat(target.self, 10);
    int32_t gap = min32(hooks.skillRankWithAttribute(self, school) - resist,
                    target.stat(target.self, 2));
    int32_t advantage = hooks.skillAptitude(self, school) + gap * 5;
    int32_t difficulty = target.stat(target.self, 9) - gap * 5;
    Outcome out = drawn ? resolve(caster, school, advantage, difficulty, magickaCost)
                        : resolveWithRolls(caster, school, advantage, difficulty,
                                           magickaCost, attackRoll, defenceRoll);

    SharedArray<int16_t> vitals = caster.vitals;
    const int32_t timer = spellId - 1;

    switch (spellId) {
        case spellid::WEAKNESS: {
            target.setEffect(target.self, 9, -2);
            target.store(target.self);
            break;
        }
        case spellid::BLOOD_SPIRIT: {
            int32_t n1 = 10 + hooks.skillRank(self, skills::CONJURATION);
            target.setEffect(target.self, 1, (int8_t)n1);
            break;
        }
        case spellid::ABSORB: {
            int32_t rank = hooks.skillRank(self, skills::CONJURATION);
            target.takeDamage(target.self, 12 + 2 * rank);
            vitals[CUR_FATIGUE] = (int16_t)(vitals[CUR_FATIGUE] + rank);
            vitals[CUR_FATIGUE] = (int16_t)min32(vitals[CUR_FATIGUE], vitals[MAX_FATIGUE]);
            vitals[CUR_HP] = (int16_t)(vitals[CUR_HP] + rank);
            vitals[CUR_HP] = (int16_t)min32(vitals[CUR_HP], vitals[MAX_HP]);
            vitals[CUR_MAGICKA] = (int16_t)(vitals[CUR_MAGICKA] + 12);
            vitals[CUR_MAGICKA] = (int16_t)min32(vitals[CUR_MAGICKA], vitals[MAX_MAGICKA]);
            break;
        }
        case spellid::DEAD_TO_DUST: {
            if (!target.isUndead(target.self)) break;
            int32_t raw = 60 * out.multiplier;
            int32_t after = max32(raw - target.stat(target.self, 8), 4);
            target.takeDamage(target.self, after * target.stat(target.self, 14) / 100);
            break;
        }
        case spellid::RIGHTEOUSNESS: {
            hooks.setSpellTimer(self, timer, -2);
            target.setEffect(target.self, 8, (int8_t)(2 * out.multiplier));
            break;
        }
        case spellid::DAMAGE: {
            int32_t raw = (25 + hooks.skillRank(self, skills::DESTRUCTION)) * out.multiplier;
            int32_t after = max32(raw - target.stat(target.self, 8), 4);
            target.takeDamage(target.self, after * target.stat(target.self, 14) / 100);
            target.store(target.self);
            break;
        }
        case spellid::FEEBLE_BLADE: {
            hooks.setSpellTimer(self, timer, -2);
            int32_t n2 = out.multiplier * (10 + hooks.skillRank(self, skills::DESTRUCTION));
            n2 = min32(n2, 255);
            target.setEffect(target.self, 4, (int8_t)n2);
            target.store(target.self);
            break;
        }
        case spellid::HARM_ARMOR: {
            hooks.setSpellTimer(self, timer, -2);
            int32_t n3 = out.multiplier * (10 + hooks.skillRank(self, skills::DESTRUCTION));
            n3 = min32(n3, 255);
            target.setEffect(target.self, 5, (int8_t)n3);
            target.store(target.self);
            break;
        }
        case spellid::DOOM_HAMMER: {
            hooks.setSpellTimer(self, timer, -1);
            hooks.attackMonster(self, hooks.monster);
            hooks.setSpellTimer(self, timer, 0);
            break;
        }
        case spellid::DRAIN: {
            hooks.setSpellTimer(self, timer, -2);
            target.setEffect(target.self, 2, 1);
            target.store(target.self);
            break;
        }
        case spellid::PARALYZE: {
            int32_t window = 10 - resist;
            if (window <= 0) break;
            window = out.multiplier * window;
            hooks.setSpellTimer(self, timer, (int8_t)window);
            target.setEffect(target.self, 6, 1);
            break;
        }
        case spellid::SANCTUARY: {
            hooks.setSpellTimer(self, timer, -2);
            hooks.setDamageBonus(self, (int16_t)(10 + hooks.skillRank(self, skills::ILLUSION)));
            break;
        }
        case spellid::BLIND: {
            hooks.setSpellTimer(self, timer, -2);
            target.setEffect(target.self, 0, (int8_t)(3 * out.multiplier));
            break;
        }
        case spellid::FEAR: {
            hooks.setSpellTimer(self, timer, -2);
            int32_t chance = out.multiplier * (60 - 5 * resist);
            chance = min32(max32(chance, 0), 100);
            target.setEffect(target.self, 3, (int8_t)chance);
            break;
        }
        case spellid::DEATH_HOWL: {
            int32_t raw = (80 - 5 * resist) * out.multiplier;
            int32_t after = max32(raw - target.stat(target.self, 8), 4);
            target.takeDamage(target.self, after * target.stat(target.self, 14) / 100);
        }
    }

    settle(caster);
}

}

void castAtMonster(const Caster &caster, const Target &target, const MonsterSpellHooks &hooks,
                   int32_t spellId, int32_t school, int32_t magickaCost) {
    castAtMonsterImpl(caster, target, hooks, spellId, school, magickaCost, true, 0, 0);
}

void castAtMonsterWithRolls(const Caster &caster, const Target &target,
                            const MonsterSpellHooks &hooks, int32_t spellId, int32_t school,
                            int32_t magickaCost, int32_t attackRoll, int32_t defenceRoll) {
    castAtMonsterImpl(caster, target, hooks, spellId, school, magickaCost, false, attackRoll,
                      defenceRoll);
}

void settle(const Caster &caster) {
    SharedArray<int16_t> vitals = caster.vitals;
    vitals[CUR_FATIGUE] =
        (int16_t)(vitals[CUR_FATIGUE] - kFatigueCost * caster.fatigueMultiplier(caster.self));
    vitals[CUR_FATIGUE] = (int16_t)max32(vitals[CUR_FATIGUE], 0);

    if (caster.hasAilment(caster.self, kCorruptionAilment)) {
        int32_t bleed = kCorruptionPercent * vitals[MAX_HP] / 100;
        if (bleed < 1) {
            bleed = 1;
        }
        vitals[CUR_HP] = (int16_t)(vitals[CUR_HP] - (int16_t)bleed);
    }
}

}
