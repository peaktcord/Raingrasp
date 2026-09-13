#ifndef COMMON_GAME_SPELLCAST_HPP
#define COMMON_GAME_SPELLCAST_HPP

#include "src/common/runtime.hpp"

namespace spellcast {

constexpr int32_t kChanceFloor = 10;
constexpr int32_t kChanceCeiling = 95;

constexpr int32_t kCorruptionAilment = 6;
constexpr int32_t kCorruptionPercent = 2;

constexpr int32_t kFatigueCost = 5;

struct Caster {
    GameRandom *random = nullptr;
    SharedArray<int16_t> vitals;

    int32_t (*fatigueMultiplier)(void *self) = nullptr;

    bool (*hasAilment)(void *self, int32_t ailment) = nullptr;

    void (*awardSkillXp)(void *self, int32_t skill, int32_t points) = nullptr;

    void *self = nullptr;
};

struct Outcome {
    int32_t roll = 0;

    int32_t multiplier = 1;
};

Outcome resolve(const Caster &caster, int32_t skill, int32_t advantage, int32_t difficulty,
                int32_t magickaCost);

Outcome resolveWithRolls(const Caster &caster, int32_t skill, int32_t advantage, int32_t difficulty,
                         int32_t magickaCost, int32_t attackRoll, int32_t defenceRoll);

struct Target {
    int32_t (*stat)(void *self, int32_t which) = nullptr;

    void (*setEffect)(void *self, int32_t index, int8_t value) = nullptr;

    void (*takeDamage)(void *self, int32_t amount) = nullptr;

    bool (*isUndead)(void *self) = nullptr;

    void (*store)(void *self) = nullptr;

    void *self = nullptr;
};

struct MonsterSpellHooks {
    int32_t (*skillRank)(void *self, int32_t skill) = nullptr;

    int32_t (*skillRankWithAttribute)(void *self, int32_t skill) = nullptr;
    int32_t (*skillAptitude)(void *self, int32_t skill) = nullptr;

    void (*setSpellTimer)(void *self, int32_t index, int8_t value) = nullptr;

    void (*setDamageBonus)(void *self, int16_t bonus) = nullptr;

    void (*attackMonster)(void *self, void *monster) = nullptr;

    void *monster = nullptr;
};

void castAtMonster(const Caster &caster, const Target &target, const MonsterSpellHooks &hooks,
                   int32_t spellId, int32_t school, int32_t magickaCost);

void castAtMonsterWithRolls(const Caster &caster, const Target &target,
                            const MonsterSpellHooks &hooks, int32_t spellId, int32_t school,
                            int32_t magickaCost, int32_t attackRoll, int32_t defenceRoll);

void settle(const Caster &caster);

}

#endif
