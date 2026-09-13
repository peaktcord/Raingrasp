#include "src/common/game/formulas.hpp"
#include "src/common/game/util.hpp"

int32_t GameFormulas::clampChance(int32_t chance) {
    return min32(max32(chance, 10), 95);
}

int32_t GameFormulas::resolveCombatRoll(int32_t attackChance, int32_t defenseChance, int32_t attackRoll,
                                     int32_t defenseRoll, bool *outDefenderSuccess) {
    bool defenseSuccess = defenseRoll <= defenseChance;
    bool attackSuccess = attackRoll <= attackChance;
    if (outDefenderSuccess != nullptr) {
        *outDefenderSuccess = defenseSuccess;
    }
    if (attackSuccess && !defenseSuccess) {
        return 3;
    }
    if (attackSuccess && defenseSuccess) {
        return (attackRoll >= defenseRoll) ? 2 : 1;
    }
    if (!attackSuccess && !defenseSuccess) {
        return (attackRoll >= defenseRoll) ? 2 : 1;
    }
    return 0;
}

int32_t GameFormulas::resolveCombatRoll(GameRandom *random, int32_t attackChance, int32_t defenseChance,
                                     bool *outDefenderSuccess) {
    int32_t defenseRoll = GameUtil::randomInt(random, 100);
    int32_t attackRoll = GameUtil::randomInt(random, 100);
    return resolveCombatRoll(attackChance, defenseChance, attackRoll, defenseRoll,
                             outDefenderSuccess);
}

int32_t GameFormulas::calcDamage(int32_t rawDamage, int32_t armorDeduction, int32_t targetMaxHp) {
    int32_t effective = max32(rawDamage - armorDeduction, 4);
    return effective * targetMaxHp / 100;
}

int32_t GameFormulas::calcSwingFatigueCost(int32_t weaponWeight) {
    return 7 * weaponWeight;
}

int32_t GameFormulas::calcFatigueRegen(int64_t deltaMillis, int32_t speed, int32_t endurance) {
    return (int32_t)(deltaMillis * (int64_t)(speed + endurance) / 2000LL);
}

void GameFormulas::restoreVitals(SharedArray<int16_t> &stats) {
    stats[2] = stats[3];
    stats[4] = stats[5];
    stats[6] = stats[7];
    stats[8] = 0;
}

void GameFormulas::calcMaxVitals(const SharedArray<int16_t> &attributes, int16_t magickaMultiplier,
                                 int16_t &outMaxHp, int16_t &outMaxMagicka,
                                 int16_t &outMaxFatigue) {
    outMaxHp = (int16_t)((attributes[0] + attributes[10]) / 2);
    outMaxMagicka = (int16_t)(magickaMultiplier * attributes[2] / 4);
    outMaxFatigue = (int16_t)(attributes[0] + attributes[4] + attributes[6] + attributes[10]);
}

int32_t GameFormulas::calcInteractionCheck(GameRandom *random, int32_t playerSkill, int16_t targetDiff,
                                        int32_t bonusStat) {
    int32_t delta = playerSkill - targetDiff;
    int32_t defenseChance = clampChance(20 - delta * 5);
    int32_t attackChance = clampChance(20 + bonusStat / 2 + delta * 5);
    return resolveCombatRoll(random, attackChance, defenseChance);
}
