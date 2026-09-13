#ifndef COMMON_GAME_MELEE_HPP
#define COMMON_GAME_MELEE_HPP

#include "src/common/runtime.hpp"

namespace melee {

struct Target {
    int16_t uid = 0;
    int8_t type = 0;

    int32_t (*stat)(void *self, int32_t which) = nullptr;

    int8_t (*effect)(void *self, int32_t index) = nullptr;

    void (*takeDamage)(void *self, int32_t amount) = nullptr;

    void (*store)(void *self) = nullptr;

    void *self = nullptr;

    int32_t statOf(int32_t which) const { return stat(self, which); }
    int8_t effectAt(int32_t index) const { return effect(self, index); }
    void damage(int32_t amount) const { takeDamage(self, amount); }
    void save() const { store(self); }
};

struct Attacker {
    GameRandom *random = nullptr;
    int32_t (*attackSkill)(void *self, bool withAttribute) = nullptr;
    int32_t (*attackAptitude)(void *self) = nullptr;
    int32_t (*attackSkillIndex)(void *self) = nullptr;
    int32_t (*weaponDamage)(void *self) = nullptr;
    int32_t (*fatigueMultiplier)(void *self) = nullptr;

    bool (*spellActive)(void *self, int32_t spell) = nullptr;
    void (*clearSpell)(void *self, int32_t spell) = nullptr;
    bool (*hasAilment)(void *self, int32_t ailment) = nullptr;

    void (*awardSkillXp)(void *self, int32_t skill, int32_t points) = nullptr;

    void (*setTargetUid)(void *self, int16_t uid) = nullptr;

    void (*clearSpellTimerRaw)(void *self, int32_t index) = nullptr;

    SharedArray<int16_t> vitals;

    void *self = nullptr;
};

void swing(const Attacker &attacker, const Target &target);

void swingWithRolls(const Attacker &attacker, const Target &target, int32_t attackRoll,
                    int32_t defenceRoll);

}

#endif
