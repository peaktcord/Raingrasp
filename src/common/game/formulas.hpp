#ifndef COMMON_GAME_FORMULAS_HPP
#define COMMON_GAME_FORMULAS_HPP

#include "src/common/runtime.hpp"

class GameFormulas {
public:
    static int32_t clampChance(int32_t chance);

    static int32_t resolveCombatRoll(GameRandom *random, int32_t attackChance, int32_t defenseChance,
                                  bool *outDefenderSuccess = nullptr);

    static int32_t resolveCombatRoll(int32_t attackChance, int32_t defenseChance, int32_t attackRoll,
                                  int32_t defenseRoll, bool *outDefenderSuccess = nullptr);

    static int32_t calcDamage(int32_t rawDamage, int32_t armorDeduction, int32_t targetMaxHp);

    static int32_t calcSwingFatigueCost(int32_t weaponWeight);

    static int32_t calcFatigueRegen(int64_t deltaMillis, int32_t speed, int32_t endurance);

    static void restoreVitals(SharedArray<int16_t> &stats);

    static void calcMaxVitals(const SharedArray<int16_t> &attributes, int16_t magickaMultiplier,
                              int16_t &outMaxHp, int16_t &outMaxMagicka, int16_t &outMaxFatigue);

    static int32_t calcInteractionCheck(GameRandom *random, int32_t playerSkill, int16_t targetDiff,
                                     int32_t bonusStat);
};

#endif
