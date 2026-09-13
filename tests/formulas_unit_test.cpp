#include <cassert>
#include <cstdio>

#include "src/common/game/formulas.hpp"
#include "src/common/game/util.hpp"

int main() {
    // 1. Clamp chance
    assert(GameFormulas::clampChance(5) == 10);
    assert(GameFormulas::clampChance(10) == 10);
    assert(GameFormulas::clampChance(50) == 50);
    assert(GameFormulas::clampChance(95) == 95);
    assert(GameFormulas::clampChance(100) == 95);

    // 2. Combat roll outcomes
    // Attack success (roll 30 <= chance 50), Defense fails (roll 80 > chance 40) -> 3 (Critical)
    assert(GameFormulas::resolveCombatRoll(50, 40, 30, 80) == 3);
    // Both succeed, attackRoll >= defenseRoll (50 >= 30) -> 2
    assert(GameFormulas::resolveCombatRoll(60, 40, 50, 30) == 2);
    // Both succeed, attackRoll < defenseRoll (20 < 40) -> 1
    assert(GameFormulas::resolveCombatRoll(60, 50, 20, 40) == 1);
    // Both fail, attackRoll >= defenseRoll (80 >= 70) -> 2
    assert(GameFormulas::resolveCombatRoll(50, 40, 80, 70) == 2);
    // Both fail, attackRoll < defenseRoll (60 < 70) -> 1
    assert(GameFormulas::resolveCombatRoll(50, 40, 60, 70) == 1);
    // Attack fails (roll 60 > chance 50), Defense succeeds (roll 30 <= chance 40) -> 0 (Miss)
    assert(GameFormulas::resolveCombatRoll(50, 40, 60, 30) == 0);

    // 3. Damage calculation
    // raw = 20, armor = 5, maxHp = 100 -> (20 - 5) * 100 / 100 = 15
    assert(GameFormulas::calcDamage(20, 5, 100) == 15);
    // raw = 10, armor = 20, maxHp = 200 -> max(10 - 20, 4) * 200 / 100 = 4 * 2 = 8 (floor at 4%)
    assert(GameFormulas::calcDamage(10, 20, 200) == 8);

    // 4. Swing fatigue cost
    assert(GameFormulas::calcSwingFatigueCost(3) == 21);
    assert(GameFormulas::calcSwingFatigueCost(1) == 7);

    // 5. Fatigue regen
    // 2000 ms, speed=50, endurance=50 -> 2000 * 100 / 2000 = 100
    assert(GameFormulas::calcFatigueRegen(2000, 50, 50) == 100);
    // 1000 ms, speed=30, endurance=50 -> 1000 * 80 / 2000 = 40
    assert(GameFormulas::calcFatigueRegen(1000, 30, 50) == 40);

    // 6. Vitals restore
    SharedArray<int16_t> vitals(9);
    vitals[0] = 1;
    vitals[1] = 0;
    vitals[2] = 10;  // curr HP
    vitals[3] = 100; // max HP
    vitals[4] = 5;   // curr Magicka
    vitals[5] = 50;  // max Magicka
    vitals[6] = 20;  // curr Fatigue
    vitals[7] = 80;  // max Fatigue
    vitals[8] = 99;  // debuffs

    GameFormulas::restoreVitals(vitals);
    assert(vitals[2] == 100);
    assert(vitals[4] == 50);
    assert(vitals[6] == 80);
    assert(vitals[8] == 0);

    // 7. Max vitals calculation
    // attributes: [0]=STR(50), [2]=INT(40), [4]=WIL(30), [6]=AGI(40), [10]=END(60)
    SharedArray<int16_t> attrs(16);
    attrs[0] = 50;
    attrs[2] = 40;
    attrs[4] = 30;
    attrs[6] = 40;
    attrs[10] = 60;
    int16_t maxHp = 0, maxMagicka = 0, maxFatigue = 0;
    GameFormulas::calcMaxVitals(attrs, 2 /* magickaMultiplier */, maxHp, maxMagicka, maxFatigue);
    assert(maxHp == (50 + 60) / 2);              // 55
    assert(maxMagicka == (2 * 40) / 4);           // 20
    assert(maxFatigue == (50 + 30 + 40 + 60));    // 180

    // 8. Interaction check with random generator initialized
    GameRandom rng(12345);
    int32_t result = GameFormulas::calcInteractionCheck(&rng, 50, 30, 20);
    assert(result >= 0 && result <= 3);

    // 9. Equal explicit seeds produce equal streams without active-context state.
    GameRandom firstRng(24680);
    GameRandom secondRng(24680);
    int32_t firstDraw = GameUtil::randomInt(&firstRng, 1000);
    int32_t secondDraw = GameUtil::randomInt(&secondRng, 1000);
    assert(firstDraw == secondDraw);

    std::printf("All GameFormulas unit tests passed!\n");
    return 0;
}
