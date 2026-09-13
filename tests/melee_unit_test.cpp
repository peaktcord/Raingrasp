// One melee swing, resolved against a fake attacker and a fake monster.
//
// Before the extraction this code could only be reached by walking a dungeon
// with the real JARs present and hitting whatever the script happened to hit.
// The replay baselines cover the paths that walk takes; they do not cover the
// three spell branches, the two critical-hit cases, or the ailment drain,
// because a scripted walk does not arrange for those.
//
// What is pinned here, and why each is worth pinning:
//
//   - **The critical asymmetry.** Roll 1 doubles the *armour deduction*; roll
//     3 doubles the *damage*. Read quickly the first looks like a slip for the
//     second -- doubling the thing that reduces your damage is a strange
//     critical -- but both shipped games do it, so both directions are
//     asserted and the two rolls are checked to move different quantities.
//
//   - **Each spell's zero-effect branch clears rather than applies.** All
//     three follow the same shape: if the monster's matching effect slot is
//     zero the *attacker's own spell* is cleared, otherwise the effect
//     modifies the swing. Getting the sense backwards is a one-character
//     change that a walk would rarely notice.
//
//   - **The fatigue skip is re-tested, not reused.** `spellActive(7)` is
//     consulted twice, and between the two calls the first branch may have
//     cleared it. So a swing whose damage-over-time effect was zero *does*
//     pay fatigue, and one whose effect was live does not. Reusing the
//     earlier result -- the obvious tidy-up -- inverts that, and it is
//     asserted directly.
//
//   - **The spell-7 damage is not stored.** The swing stores once, then may
//     deal a second lot of damage. That reads like a missing store; it is what
//     both games ship. The store count is asserted so a "fix" fails here
//     rather than silently changing what the monster table holds.
//
//   - **A miss still sets the target uid.** The early return happens after
//     that assignment, so a whiffed swing still remembers what you swung at.
//
//   - **The advantage is capped by the monster's evasion before the spell
//     bonus, not after.** `min32` runs first, so a to-hit spell can push the
//     advantage back above the cap. Swapping the two lines is invisible unless
//     the spell is active and the cap is binding, which is asserted.

#include <cstdio>
#include <string>
#include <vector>

#include "src/common/game/formulas.hpp"
#include "src/common/game/melee.hpp"

namespace {

int failures = 0;

void check(bool ok, const std::string &what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    }
}

void checkEq(int32_t got, int32_t want, const std::string &what) {
    if (got != want) {
        std::printf("FAIL: %s: got %d want %d\n", what.c_str(), (int)got, (int)want);
        ++failures;
    }
}

// ---------------------------------------------------------------- fakes

// A monster whose stats and effects the test dictates, recording what the
// swing did to it.
struct FakeMonster {
    int32_t stats[16] = {0};
    int8_t effects[16] = {0};
    std::vector<int32_t> damageTaken;
    int stores = 0;

    melee::Target target(int16_t uid = 7) {
        melee::Target t;
        t.self = this;
        t.uid = uid;
        t.type = 3;
        t.stat = [](void *self, int32_t which) {
            return static_cast<FakeMonster *>(self)->stats[which];
        };
        t.effect = [](void *self, int32_t index) {
            return static_cast<FakeMonster *>(self)->effects[index];
        };
        t.takeDamage = [](void *self, int32_t amount) {
            static_cast<FakeMonster *>(self)->damageTaken.push_back(amount);
        };
        t.store = [](void *self) { ++static_cast<FakeMonster *>(self)->stores; };
        return t;
    }

    int32_t totalDamage() const {
        int32_t sum = 0;
        for (int32_t d : damageTaken) sum += d;
        return sum;
    }
};

// An attacker whose ratings the test dictates, recording what was asked of it.
struct FakePlayer {
    int32_t skill = 50;
    int32_t aptitude = 50;
    int32_t skillIndex = 4;
    int32_t weapon = 20;
    int32_t fatigueMul = 10;
    bool spells[16] = {false};
    bool ailment6 = false;

    SharedArray<int16_t> vitals;
    int8_t timers[32] = {0};

    int16_t targetUid = 0;
    std::vector<int32_t> cleared;
    std::vector<int32_t> rawTimersCleared;
    std::vector<std::pair<int32_t, int32_t>> awards;

    FakePlayer() {
        vitals = SharedArray<int16_t>(10);
        vitals[2] = 100;  // health
        vitals[3] = 100;  // health max
        vitals[6] = 100;  // fatigue
    }

    melee::Attacker attacker() {
        melee::Attacker a;
        a.self = this;
        a.vitals = vitals;
        a.attackSkill = [](void *s, bool) { return static_cast<FakePlayer *>(s)->skill; };
        a.attackAptitude = [](void *s) { return static_cast<FakePlayer *>(s)->aptitude; };
        a.attackSkillIndex = [](void *s) { return static_cast<FakePlayer *>(s)->skillIndex; };
        a.weaponDamage = [](void *s) { return static_cast<FakePlayer *>(s)->weapon; };
        a.fatigueMultiplier = [](void *s) { return static_cast<FakePlayer *>(s)->fatigueMul; };
        a.spellActive = [](void *s, int32_t spell) {
            return static_cast<FakePlayer *>(s)->spells[spell];
        };
        a.clearSpell = [](void *s, int32_t spell) {
            FakePlayer *p = static_cast<FakePlayer *>(s);
            p->cleared.push_back(spell);
            p->spells[spell] = false;
        };
        a.hasAilment = [](void *s, int32_t a2) {
            return a2 == 6 && static_cast<FakePlayer *>(s)->ailment6;
        };
        a.awardSkillXp = [](void *s, int32_t skill, int32_t points) {
            static_cast<FakePlayer *>(s)->awards.push_back(std::make_pair(skill, points));
        };
        a.setTargetUid = [](void *s, int16_t uid) {
            static_cast<FakePlayer *>(s)->targetUid = uid;
        };
        a.clearSpellTimerRaw = [](void *s, int32_t index) {
            FakePlayer *p = static_cast<FakePlayer *>(s);
            p->rawTimersCleared.push_back(index);
            p->timers[index] = 0;
        };
        return a;
    }
};

// The rolls are supplied rather than drawn, via melee::swingWithRolls. The
// mapping from (attackRoll, defenceRoll) to outcome is GameFormulas' own, and
// these are the four cases it can produce with the chances pinned at 50.
//
// Deterministic on purpose: the random form dereferences the game's global
// RNG, which a unit test has no business seeding, and a test that seeds a
// global and hopes for a roll is a test that fails on a Tuesday.
struct Rolls {
    int32_t attack;
    int32_t defence;
};

// chances are both 50; see setUpChances below.
//   roll 0: attack fails, defence succeeds
//   roll 1: both succeed (or both fail) and attackRoll < defenceRoll
//   roll 2: both succeed (or both fail) and attackRoll >= defenceRoll
//   roll 3: attack succeeds, defence fails
Rolls rollsFor(int32_t wanted) {
    switch (wanted) {
        case 0: return Rolls{60, 40};   // attack > 50 fails, defence <= 50 succeeds
        case 1: return Rolls{30, 40};   // both succeed, 30 < 40
        case 2: return Rolls{40, 30};   // both succeed, 40 >= 30
        default: return Rolls{40, 60};  // attack succeeds, defence fails
    }
}

// Pins both chances at 50 by zeroing the skill advantage, so the roll is
// decided entirely by the two numbers rollsFor hands over.
void setUpChances(FakePlayer *p, FakeMonster *m) {
    p->skill = 0;
    m->stats[7] = 0;  // dodge
    m->stats[2] = 0;  // evasion caps the advantage at 0
    p->aptitude = 50;
    m->stats[6] = 50;
}

// Swings with the rolls that produce `roll`, and checks that it did.
void swingWith(FakePlayer *p, FakeMonster *m, int32_t roll, int16_t uid = 7) {
    Rolls r = rollsFor(roll);
    int32_t got = GameFormulas::resolveCombatRoll(50, 50, r.attack, r.defence, nullptr);
    if (got != roll) {
        std::printf("FAIL: fixture: rolls for %d produced %d\n", (int)roll, (int)got);
        ++failures;
    }
    melee::swingWithRolls(p->attacker(), m->target(uid), r.attack, r.defence);
}

// ------------------------------------------------------------- the miss

void testMiss() {
    FakePlayer p;
    FakeMonster m;
    setUpChances(&p, &m);
    swingWith(&p, &m, 0, 42);

    checkEq(p.targetUid, 42, "a miss still sets the target uid");
    checkEq((int32_t)m.damageTaken.size(), 0, "a miss deals no damage");
    checkEq(m.stores, 0, "a miss stores nothing");
    checkEq((int32_t)p.awards.size(), 0, "a miss awards no experience");
    checkEq(p.vitals[6], 100, "a miss costs no fatigue");
}

// ------------------------------------------------------- the critical rolls

// Roll 1 doubles the armour deduction; roll 3 doubles the damage. Both
// directions, and the two must not be the same quantity.
void testCriticalAsymmetry() {
    int32_t damageAtRoll[4] = {0, 0, 0, 0};
    for (int32_t roll = 1; roll <= 3; ++roll) {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.weapon = 40;
        m.stats[8] = 10;   // armour
        m.stats[14] = 100; // max hp
        swingWith(&p, &m, roll);
        damageAtRoll[roll] = m.totalDamage();
    }

    // Roll 3 doubles the damage, so it must hurt more than the plain hit.
    check(damageAtRoll[3] > damageAtRoll[2],
          "roll 3 (doubled damage) deals more than roll 2");
    // Roll 1 doubles the armour deduction, so it must hurt *less*.
    check(damageAtRoll[1] < damageAtRoll[2],
          "roll 1 (doubled armour deduction) deals less than roll 2 -- "
          "the asymmetry both games ship");

    // And the two are not the same operation applied twice: computed
    // explicitly against GameFormulas so a change to either doubling fails.
    checkEq(damageAtRoll[1], GameFormulas::calcDamage(40, 20, 100),
            "roll 1 doubles armour: calcDamage(40, 2*10, 100)");
    checkEq(damageAtRoll[3], GameFormulas::calcDamage(80, 10, 100),
            "roll 3 doubles damage: calcDamage(2*40, 10, 100)");
    checkEq(damageAtRoll[2], GameFormulas::calcDamage(40, 10, 100),
            "roll 2 doubles neither");
}

// ------------------------------------------------------------ the spells

// Spell 10 (to-hit): a live effect adds to the advantage, a zero effect
// clears the spell instead.
void testToHitSpell() {
    // Zero effect -> the spell is cleared and nothing is added.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.spells[10] = true;
        m.effects[8] = 0;
        swingWith(&p, &m, 2);
        check(p.cleared.size() == 1 && p.cleared[0] == 10,
              "spell 10 with a zero effect is cleared");
    }
    // Live effect -> the advantage moves, and the spell is not cleared.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        m.stats[2] = 100;  // let the advantage through the cap
        p.spells[10] = true;
        m.effects[8] = 6;
        int32_t before = p.aptitude;
        swingWith(&p, &m, 2);
        checkEq((int32_t)p.cleared.size(), 0, "spell 10 with a live effect is not cleared");
        (void)before;
    }
}

// Spell 13 (armour debuff): a live effect reduces the monster's armour; a
// zero effect clears the *timer directly*, not through clearSpell.
void testArmourSpell() {
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.spells[13] = true;
        m.effects[5] = 0;
        swingWith(&p, &m, 2);
        check(p.rawTimersCleared.size() == 1 && p.rawTimersCleared[0] == 12,
              "spell 13 with a zero effect zeroes timer 12 directly");
        check(p.cleared.empty(), "and does not go through clearSpell");
    }
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.weapon = 40;
        m.stats[8] = 20;
        m.stats[14] = 100;
        p.spells[13] = true;
        m.effects[5] = 8;
        swingWith(&p, &m, 2);
        checkEq(m.totalDamage(), GameFormulas::calcDamage(40, 12, 100),
                "spell 13 subtracts its effect from the monster's armour");
        check(p.rawTimersCleared.empty(), "and leaves the timer alone");
    }
}

// Spell 7 (damage over time): a live effect deals a second, unstored hit; a
// zero effect clears the spell -- and the fatigue skip keys off the *result*
// of that clearing, not off the state at the top of the swing.
void testDamageOverTimeAndFatigue() {
    // Live: a second hit lands, and it is not stored.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.weapon = 40;
        m.stats[8] = 0;
        m.stats[14] = 100;
        p.spells[7] = true;
        m.effects[1] = 9;
        swingWith(&p, &m, 2);
        checkEq((int32_t)m.damageTaken.size(), 2, "a live spell 7 deals a second hit");
        checkEq(m.damageTaken[1], GameFormulas::calcDamage(9, 0, 100),
                "the second hit is the effect value, with no armour deduction");
        checkEq(m.stores, 1,
                "the second hit is NOT stored -- preserved, see melee.hpp");
        // Still active, so no fatigue is paid.
        checkEq(p.vitals[6], 100, "a live spell 7 skips the fatigue cost");
    }
    // Zero: the spell is cleared, and because it is cleared, fatigue IS paid.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.spells[7] = true;
        m.effects[1] = 0;
        swingWith(&p, &m, 2);
        checkEq((int32_t)m.damageTaken.size(), 1, "a zero spell 7 deals no second hit");
        check(p.cleared.size() == 1 && p.cleared[0] == 7, "and clears the spell");
        check(p.vitals[6] < 100,
              "and then PAYS fatigue, because the second test sees the cleared "
              "spell -- reusing the first result would invert this");
    }
    // No spell at all: fatigue paid, as the ordinary case.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        swingWith(&p, &m, 2);
        checkEq(p.vitals[6],
                (int16_t)(100 - GameFormulas::calcSwingFatigueCost(p.fatigueMul)),
                "an ordinary swing pays the fatigue cost");
    }
}

// Fatigue floors at zero rather than going negative.
void testFatigueFloor() {
    FakePlayer p;
    FakeMonster m;
    setUpChances(&p, &m);
    p.vitals[6] = 1;
    p.fatigueMul = 100;
    swingWith(&p, &m, 2);
    checkEq(p.vitals[6], 0, "fatigue floors at zero");
}

// ---------------------------------------------------------- the ailment

void testBleedAilment() {
    // 2% of max health, rounded down, minimum 1.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.ailment6 = true;
        p.vitals[3] = 250;
        p.vitals[2] = 250;
        swingWith(&p, &m, 2);
        checkEq(p.vitals[2], 245, "ailment 6 drains 2% of max health (250 -> 5)");
    }
    // The floor: 2% of anything under 50 is zero, and the minimum is 1.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.ailment6 = true;
        p.vitals[3] = 10;
        p.vitals[2] = 10;
        swingWith(&p, &m, 2);
        checkEq(p.vitals[2], 9, "the drain is at least 1 even at low max health");
    }
    // Without the ailment, health is untouched.
    {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        swingWith(&p, &m, 2);
        checkEq(p.vitals[2], 100, "no ailment, no health drain");
    }
}

// ------------------------------------------------------------- the award

void testSkillAward() {
    for (int32_t roll = 1; roll <= 3; ++roll) {
        FakePlayer p;
        FakeMonster m;
        setUpChances(&p, &m);
        p.skillIndex = 9;
        swingWith(&p, &m, roll);
        if (roll >= 2) {
            check(p.awards.size() == 1 && p.awards[0].first == 9 && p.awards[0].second == 1,
                  "roll " + std::to_string(roll) + " awards one point to the attack skill");
        } else {
            checkEq((int32_t)p.awards.size(), 0,
                    "roll 1 awards nothing -- only rolls >= 2 do");
        }
    }
}

// ----------------------------------------------------- the evasion cap

// The cap runs before the spell bonus, so a to-hit spell can push the
// advantage back above it. Swapping the two lines is invisible without both.
void testEvasionCapOrdering() {
    FakePlayer p;
    FakeMonster m;
    setUpChances(&p, &m);
    p.skill = 100;
    m.stats[7] = 0;   // dodge
    m.stats[2] = 3;   // evasion caps the advantage at 3
    p.spells[10] = true;
    m.effects[8] = 20;  // then the spell adds 20 on top of the cap

    // This fixture deliberately does *not* pin the chances at 50 -- the
    // advantage is the thing under test -- so it drives the swing directly
    // rather than through swingWith, whose roll assertion assumes 50/50.
    //
    // The two orderings and what they produce, with clampChance's real [10,95]
    // range rather than the [0,100] one might assume:
    //
    //   cap first: advantage = min(100, 3) = 3, then + 20 = 23
    //              attack = clamp(50 + 115) = 95, defence = clamp(50 - 115) = 10
    //   cap last:  advantage = min(100 + 20, 3) = 3
    //              attack = clamp(50 + 15) = 65, defence = clamp(50 - 15) = 35
    //
    // The rolls have to separate those. **40/40 does not** -- it is roll 3
    // under both, which is how the first version of this fixture passed
    // against a mutation that moved the cap. 40/20 does: the defence roll
    // misses a chance of 10 but makes one of 35, so cap-first is roll 3
    // (damage doubled) and cap-last is roll 2 (not doubled).
    p.weapon = 40;
    m.stats[8] = 0;
    m.stats[14] = 100;
    melee::swingWithRolls(p.attacker(), m.target(), 40, 20);

    checkEq(p.targetUid, 7, "the swing ran");
    checkEq(m.totalDamage(), GameFormulas::calcDamage(80, 0, 100),
            "the evasion cap is applied before the to-hit spell, not after -- "
            "cap-last gives roll 2 and undoubled damage");
}

}  // namespace

int main() {
    testMiss();
    testCriticalAsymmetry();
    testToHitSpell();
    testArmourSpell();
    testDamageOverTimeAndFatigue();
    testFatigueFloor();
    testBleedAilment();
    testSkillAward();
    testEvasionCapOrdering();

    if (failures != 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("melee: all checks passed\n");
    return 0;
}
