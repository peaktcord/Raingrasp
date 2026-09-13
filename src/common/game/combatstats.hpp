#ifndef COMMON_GAME_COMBATSTATS_HPP
#define COMMON_GAME_COMBATSTATS_HPP

#include "src/common/runtime.hpp"

#include "src/common/game/equipment.hpp"
#include "src/common/game/skills.hpp"

namespace combatstats {

using equipment::BOOTS;
using equipment::CUIRASS;
using equipment::GLOVES;
using equipment::HELMET;
using equipment::SHIELD;
using equipment::WEAPON;

const int32_t kSlotCount = equipment::kReadableSlotCount;

using skills::ALTERATION;
using skills::AXE;
using skills::BLUNT;
using skills::CONJURATION;
using skills::DESTRUCTION;
using skills::HEAVY_ARMOR;
using skills::LIGHT_ARMOR;
using skills::LONG_BLADE;
using skills::SHORT_BLADE;

enum Category {
    CAT_AXE = 1,
    CAT_BLUNT = 2,
    CAT_LONG_SWORD = 3,
    CAT_SHORT_SWORD = 4,
    CAT_HEAVY_ARMOR = 5,
    CAT_LIGHT_ARMOR = 6,
};

enum Effect {
    STRENGTH = 1,
    SHIELD_SPELL = 2,
    FORTIFY = 5,
    SPECTRAL = 6,
    BOUND_WEAPON = 14,
    BOUND_ARMOR = 17,
};

struct Combatant {
    const int8_t *equipped = nullptr;

    int32_t (*rank)(const void *self, int32_t skill, bool withAttribute) = nullptr;

    int32_t (*aptitude)(const void *self, int32_t skill) = nullptr;

    bool (*effect)(const void *self, int32_t effect) = nullptr;

    const void *self = nullptr;

    int32_t damageBonus = 0;
    bool potionAttack = false;
    bool potionDefence = false;

    int32_t rankOf(int32_t skill, bool withAttribute) const {
        return rank(self, skill, withAttribute);
    }
    int32_t aptitudeOf(int32_t skill) const { return aptitude(self, skill); }
    bool active(int32_t e) const { return effect(self, e); }
};

int32_t weaponSkillForCategory(int32_t category);

int32_t cuirassSkillForCategory(int32_t category);

int32_t bestWeaponSkill(const Combatant &c);

int32_t attackSkillIndex(const Combatant &c);

int32_t defenceSkillIndex(const Combatant &c);

int32_t attackSkill(const Combatant &c, bool withAttribute);

int32_t defenceSkill(const Combatant &c, bool withAttribute);

int32_t attackAptitude(const Combatant &c);

int32_t defenceAptitude(const Combatant &c);

int32_t weaponDamage(const Combatant &c);

int32_t armourRating(const Combatant &c);

}

#endif
