#include "src/common/game/combatstats.hpp"

#include "src/common/game/items.hpp"

namespace combatstats {
namespace {

int32_t categoryIn(const Combatant &c, int32_t slot) {
    int8_t id = c.equipped[slot];
    if (id == 0) return 0;
    return wrappingAbs(Items::stat(Items::CATEGORY, (int32_t)id));
}

int32_t ratingIn(const Combatant &c, int32_t slot) {
    return Items::stat(Items::RATING, (int32_t)c.equipped[slot]);
}

}

int32_t weaponSkillForCategory(int32_t category) {
    if (category == CAT_AXE) return AXE;
    if (category == CAT_BLUNT) return BLUNT;
    if (category == CAT_LONG_SWORD) return LONG_BLADE;
    return SHORT_BLADE;
}

int32_t cuirassSkillForCategory(int32_t category) {
    return category == CAT_HEAVY_ARMOR ? HEAVY_ARMOR : LIGHT_ARMOR;
}

int32_t bestWeaponSkill(const Combatant &c) {
    static const int32_t kWeaponSkills[] = {AXE, BLUNT, LONG_BLADE, SHORT_BLADE};

    int32_t best = kWeaponSkills[0];
    int32_t bestRank = c.rankOf(kWeaponSkills[0], false);
    for (int32_t n = 1; n < 4; ++n) {
        int32_t rank = c.rankOf(kWeaponSkills[n], false);
        if (rank > bestRank) {
            bestRank = rank;
            best = kWeaponSkills[n];
        }
    }
    return best;
}

int32_t attackSkillIndex(const Combatant &c) {
    if (c.active(SPECTRAL)) return bestWeaponSkill(c);
    int32_t category = categoryIn(c, WEAPON);
    if (category == 0) return -1;
    return weaponSkillForCategory(category);
}

int32_t defenceSkillIndex(const Combatant &c) {
    int32_t category = categoryIn(c, CUIRASS);
    if (category == 0) return -1;
    return cuirassSkillForCategory(category);
}

int32_t attackSkill(const Combatant &c, bool withAttribute) {
    if (c.active(BOUND_WEAPON)) return 5 + c.rankOf(DESTRUCTION, false);

    int32_t total = 0;
    if (c.active(SPECTRAL)) {
        total = c.rankOf(bestWeaponSkill(c), withAttribute);
    } else {
        int32_t category = categoryIn(c, WEAPON);
        total = category == 0 ? 0 : c.rankOf(weaponSkillForCategory(category), withAttribute);
    }

    if (c.active(FORTIFY)) total += c.rankOf(ALTERATION, false);
    return total;
}

int32_t defenceSkill(const Combatant &c, bool withAttribute) {
    int32_t category = categoryIn(c, CUIRASS);
    if (category == 0) return 0;
    return c.rankOf(cuirassSkillForCategory(category), withAttribute);
}

int32_t attackAptitude(const Combatant &c) {
    if (c.active(SPECTRAL) || c.active(BOUND_WEAPON)) {
        return c.aptitudeOf(bestWeaponSkill(c));
    }
    int32_t category = categoryIn(c, WEAPON);
    if (category == 0) return 20;
    return c.aptitudeOf(weaponSkillForCategory(category));
}

int32_t defenceAptitude(const Combatant &c) {
    int32_t category = categoryIn(c, CUIRASS);
    if (category == 0) return 20;
    return c.aptitudeOf(cuirassSkillForCategory(category));
}

int32_t weaponDamage(const Combatant &c) {
    int32_t damage;
    if (c.active(BOUND_WEAPON)) {
        damage = 5 + c.rankOf(DESTRUCTION, false);
    } else if (c.active(SPECTRAL)) {
        damage = 20 + c.rankOf(CONJURATION, false);
    } else {
        damage = c.equipped[WEAPON] != 0 ? ratingIn(c, WEAPON) : 0;
    }

    if (c.active(STRENGTH)) damage += 10 + c.rankOf(ALTERATION, false);
    if (c.potionAttack) damage += 25;
    return damage;
}

int32_t armourRating(const Combatant &c) {
    static const int32_t kWeights[] = {4, 2, 2, 1, 1};

    int32_t total = 0;
    for (int32_t slot = CUIRASS; slot < kSlotCount; ++slot) {
        if (c.equipped[slot] != 0) total += kWeights[slot - CUIRASS] * ratingIn(c, slot);
    }

    total /= 10;

    if (c.active(SHIELD_SPELL)) total += 10 + c.rankOf(ALTERATION, false);
    if (c.active(BOUND_ARMOR)) total += c.damageBonus;
    if (c.potionDefence) total += 15;
    return total;
}

}
