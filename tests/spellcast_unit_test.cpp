// The shared halves of a spell cast, driven against a fake caster.
//
// **This test exists because nothing else could see this code.** Before it was
// written, the magicka a failed cast costs was changed from three times the
// spell's cost to seven times, and all nineteen tests in `//:test_quick`
// passed -- the frame baselines, both replays, both player dumps, both save
// dumps. `player_dump` never casts a spell, and the scripted replays walk the
// dungeons without casting one either. The whole cast path, in both games, was
// unprotected.
//
// That is the same finding Phase 5 recorded about the levelling fixture, in a
// different place: a baseline suite that passes is not evidence that a change
// was safe unless something in it actually exercises the changed code. The
// extraction of `spellcast` was not safe to make until this existed.
//
// What is pinned here, and why each is worth pinning:
//
//   - **The magicka ladder is not monotonic.** A total failure costs *three
//     times* the spell's cost, a partial one and a half, and both successes
//     the base. So failing is the expensive outcome, and a critical is not a
//     discount -- it doubles the effect instead. All four rungs are asserted
//     against each other, so swapping any two fails.
//
//   - **The critical returns a multiplier, not cheaper magicka.** Roll 3
//     costs exactly what roll 2 costs and differs only in `multiplier`. Both
//     halves of that are asserted, because reading the ladder quickly invites
//     "a critical should cost less".
//
//   - **Magicka is charged before the spell resolves, and floored at zero.**
//     A caster who cannot afford a spell still casts it and lands on zero
//     rather than going negative. Asserted from below.
//
//   - **Experience is awarded on any success, ahead of the effect.** `>= 2`
//     covers both success grades. A failure awards nothing. Asserted as a
//     count, so an award moved inside a branch fails here.
//
//   - **The clamp is [10, 95], not [0, 100].** A hopeless cast still succeeds
//     one time in ten and a certain one still fails one in twenty. Driven
//     with rolls outside the clamped band, which is the only way to separate
//     the two -- the lesson Phase 5 learned twice, in the armour fixture and
//     again in the wrapped-row frame.
//
//   - **The corruption bleed is two percent of *maximum* health, never zero,
//     and not floored.** Three separate traps: reading current health instead
//     of maximum is invisible on a fresh character where they are equal; the
//     round-down would silently disable it below fifty maximum health; and a
//     corrupted caster can cast themselves to death in both games, so adding
//     the "obvious" floor is a behaviour change.
//
//   - **Fatigue is floored at zero and scaled by the caster's multiplier.**
//     Both asserted, and the multiplier is set to something other than one so
//     that dropping it entirely fails.
//
//   - **The twenty-branch monster switch**, whose interesting property is not
//     any one branch but which branches store and which do not. Only five of
//     the seventeen call `store`; the rest write an effect slot and leave it,
//     so the effect lands on a record the next decode overwrites. That reads
//     like a missing store in twelve places and is what both games ship, so
//     the storing set is asserted exactly rather than sampled -- adding a
//     "missing" store fails here.
//
//     The effect slots are same-typed neighbours in one array, which is the
//     hazard `player_dump` exists for, so each branch is checked to write its
//     own slot *and leave every other alone* rather than merely to write
//     something.
//
// **Every roll here is supplied, never drawn.** `resolve` and `castAtMonster`
// each have a `...WithRolls` twin, and only the twins are reachable from a
// test: the drawn forms call `GameUtil::randomInt`, which dereferences the
// game's global `GameRandom` -- a null pointer outside a running game. Calling the
// drawn form from a unit test does not fail, it segfaults, which is a much
// worse failure mode than a wrong number because it looks like a broken build.
// `melee` carries the same pair for the same reason.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "src/common/game/skills.hpp"
#include "src/common/game/spellcast.hpp"

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

// A caster whose vitals the test dictates, recording what the cast did.
struct FakeCaster {
    SharedArray<int16_t> vitals = SharedArray<int16_t>(10);
    int32_t fatigueMult = 1;
    bool corrupted = false;
    std::vector<std::pair<int32_t, int32_t>> awards;

    FakeCaster() {
        vitals[2] = 100;  // health
        vitals[3] = 100;  // health max
        vitals[4] = 100;  // magicka
        vitals[6] = 100;  // fatigue
    }

    spellcast::Caster caster() {
        spellcast::Caster c;
        c.self = this;
        c.vitals = vitals;
        c.fatigueMultiplier = [](void *self) {
            return static_cast<FakeCaster *>(self)->fatigueMult;
        };
        // Ailment 6 by number, not by `kCorruptionAilment`. A fake that answers
        // through the same constant the code asks with agrees with it whatever
        // the constant says -- the sweep caught that too.
        c.hasAilment = [](void *self, int32_t ailment) {
            return ailment == 6 && static_cast<FakeCaster *>(self)->corrupted;
        };
        c.awardSkillXp = [](void *self, int32_t skill, int32_t points) {
            static_cast<FakeCaster *>(self)->awards.push_back({skill, points});
        };
        return c;
    }
};

// The rolls are supplied rather than drawn, via resolveWithRolls -- the drawn
// form reaches the game's global RNG, and a test that seeds a global and hopes
// for a roll is a test that fails on a Tuesday. Same shape as the melee test.
//
// With both chances pinned at 50 (see castWith), GameFormulas maps the pair to:
//   roll 0: attack fails, defence succeeds
//   roll 1: both succeed (or both fail) and attackRoll < defenceRoll
//   roll 2: both succeed (or both fail) and attackRoll >= defenceRoll
//   roll 3: attack succeeds, defence fails
struct Rolls {
    int32_t attack;
    int32_t defence;
};

Rolls rollsFor(int32_t wanted) {
    switch (wanted) {
        case 0: return Rolls{60, 40};
        case 1: return Rolls{30, 40};
        case 2: return Rolls{40, 30};
        default: return Rolls{40, 60};
    }
}

// Casts with both chances at 50, so the roll is decided entirely by the pair
// rollsFor hands over, and checks that it produced the roll asked for.
spellcast::Outcome castWith(FakeCaster *p, int32_t roll, int32_t magickaCost, int32_t skill = 4) {
    Rolls r = rollsFor(roll);
    spellcast::Outcome out =
        spellcast::resolveWithRolls(p->caster(), skill, 50, 50, magickaCost, r.attack, r.defence);
    checkEq(out.roll, roll, "roll " + std::to_string(roll) + " reproduced");
    return out;
}

// ---------------------------------------------------------------- the ladder

// All four rungs, asserted against each other rather than as bare numbers, so
// that swapping any two fails rather than merely moving a constant.
void testMagickaLadder() {
    const int32_t cost = 8;

    int32_t spent[4];
    for (int32_t roll = 0; roll < 4; ++roll) {
        FakeCaster p;
        castWith(&p, roll, cost);
        spent[roll] = 100 - p.vitals[4];
    }

    checkEq(spent[0], 3 * cost, "total failure costs triple");
    checkEq(spent[1], 3 * cost / 2, "partial failure costs one and a half");
    checkEq(spent[2], cost, "success costs the base");
    checkEq(spent[3], cost, "a critical costs the base, same as a success");

    // The relations, so a change that preserves one number and not the shape
    // still fails.
    check(spent[0] > spent[1], "failing outright costs more than failing partly");
    check(spent[1] > spent[2], "failing partly costs more than succeeding");
    check(spent[3] == spent[2], "a critical is not a magicka discount");
}

// The halving is of the *tripled* cost, in integer arithmetic, so an odd cost
// rounds down. 3*5/2 = 7, not 8 and not 6.
void testPartialFailureRoundsDown() {
    FakeCaster p;
    castWith(&p, 1, 5);
    checkEq(100 - p.vitals[4], 7, "partial failure of a 5-cost spell takes 7, rounding down");
}

// A critical differs from a success only in the multiplier.
void testCriticalDoublesTheEffect() {
    FakeCaster success;
    spellcast::Outcome a = castWith(&success, 2, 4);
    FakeCaster critical;
    spellcast::Outcome b = castWith(&critical, 3, 4);

    checkEq(a.multiplier, 1, "a plain success has multiplier 1");
    checkEq(b.multiplier, 2, "a critical has multiplier 2");
    checkEq(success.vitals[4], critical.vitals[4], "and costs the same magicka");
}

// Every failing rung leaves the multiplier at 1.
void testFailuresDoNotMultiply() {
    for (int32_t roll = 0; roll < 3; ++roll) {
        FakeCaster p;
        spellcast::Outcome out = castWith(&p, roll, 4);
        checkEq(out.multiplier, 1, "roll " + std::to_string(roll) + " does not multiply");
    }
}

// The cast goes off even when it cannot be paid for, and magicka lands on
// zero rather than going negative.
void testMagickaFloor() {
    FakeCaster p;
    p.vitals[4] = 5;
    castWith(&p, 0, 20);  // would cost 60
    checkEq(p.vitals[4], 0, "magicka floors at zero rather than going negative");
}

// ---------------------------------------------------------------- experience

// Both success grades train the school; neither failure does.
void testExperienceOnSuccessOnly() {
    for (int32_t roll = 0; roll < 4; ++roll) {
        FakeCaster p;
        castWith(&p, roll, 4, /*skill=*/9);
        if (roll >= 2) {
            checkEq((int32_t)p.awards.size(), 1, "roll " + std::to_string(roll) + " awards once");
            if (!p.awards.empty()) {
                checkEq(p.awards[0].first, 9, "awarded to the spell's own school");
                checkEq(p.awards[0].second, 1, "awarded one point");
            }
        } else {
            checkEq((int32_t)p.awards.size(), 0,
                    "roll " + std::to_string(roll) + " awards nothing");
        }
    }
}

// ---------------------------------------------------------------- the clamp

// The band is [10, 95], not [0, 100]. Driven from outside it in both
// directions: a hopeless cast still succeeds sometimes, and a certain one
// still fails sometimes. Rolls are chosen to land in the gap between the two
// candidate bands -- inside [0,100] they would decide the cast the other way,
// which is the only thing that separates the two clamps.
void testChanceClamp() {
    {
        // Advantage far below the floor. Clamped to 10, so an attack roll of 5
        // still succeeds; unclamped at 0 it could not.
        FakeCaster p;
        spellcast::Outcome out =
            spellcast::resolveWithRolls(p.caster(), 4, -40, 50, 4, /*attack=*/5, /*defence=*/60);
        checkEq(out.roll, 3, "a hopeless cast still lands its one-in-ten");
    }
    {
        // Advantage far above the ceiling. Clamped to 95, so an attack roll of
        // 98 fails; unclamped at 100 it would succeed. The defence roll is put
        // *inside* its own chance so that it succeeds -- attack-fails plus
        // defence-succeeds is the only pair that reads back as roll 0, and
        // with both failing the result would be 2 whichever way the attack
        // clamp went, which is exactly the vacuous fixture this test warns
        // about elsewhere.
        FakeCaster p;
        spellcast::Outcome out =
            spellcast::resolveWithRolls(p.caster(), 4, 140, 50, 4, /*attack=*/98, /*defence=*/40);
        checkEq(out.roll, 0, "a certain cast still misses its one-in-twenty");
    }
}

// ---------------------------------------------------------------- settle

// Fatigue costs five, scaled by the caster's multiplier. The multiplier is set
// to something other than 1 so that dropping it fails.
// The 15 is written out rather than computed from `kFatigueCost`, which is the
// whole point: an expectation phrased in terms of the constant it is checking
// moves with the constant and asserts nothing. The mutation sweep caught this
// -- `kFatigueCost = 6` survived the first version of this test.
void testFatigueCost() {
    FakeCaster p;
    p.fatigueMult = 3;
    spellcast::settle(p.caster());
    checkEq(100 - p.vitals[6], 15, "fatigue costs five, times the caster's multiplier of three");

    // And with the multiplier at one, so the two factors are separable: a
    // single case cannot tell 5x3 from 15x1.
    FakeCaster q;
    q.fatigueMult = 1;
    spellcast::settle(q.caster());
    checkEq(100 - q.vitals[6], 5, "fatigue costs five at a multiplier of one");
}

void testFatigueFloor() {
    FakeCaster p;
    p.vitals[6] = 2;
    p.fatigueMult = 4;
    spellcast::settle(p.caster());
    checkEq(p.vitals[6], 0, "fatigue floors at zero");
}

// An uncorrupted caster loses no health.
void testNoBleedWithoutCorruption() {
    FakeCaster p;
    spellcast::settle(p.caster());
    checkEq(p.vitals[2], 100, "an uncorrupted caster loses no health");
}

// Two percent of *maximum* health, not current. Set the two apart, which is
// the only arrangement that can tell them apart -- they are equal by
// construction on a fresh character, the same trap the vitals sweep exists for.
void testBleedReadsMaximumHealth() {
    FakeCaster p;
    p.corrupted = true;
    p.vitals[3] = 400;  // maximum
    p.vitals[2] = 50;   // current, deliberately different
    spellcast::settle(p.caster());
    checkEq(p.vitals[2], 50 - 8, "the bleed is two percent of maximum, not current");
}

// Rounded down, but never to nothing: below fifty maximum health the two
// percent is zero and the floor of one takes over.
void testBleedNeverRoundsToZero() {
    FakeCaster p;
    p.corrupted = true;
    p.vitals[3] = 20;
    p.vitals[2] = 20;
    spellcast::settle(p.caster());
    checkEq(p.vitals[2], 19, "a small maximum still bleeds one, not zero");
}

// Not floored: both games let a corrupted caster cast themselves to death.
void testBleedIsNotFloored() {
    FakeCaster p;
    p.corrupted = true;
    p.vitals[3] = 100;
    p.vitals[2] = 1;
    spellcast::settle(p.caster());
    checkEq(p.vitals[2], -1, "health is not floored: a corrupted caster can cast themselves dead");
}


// ------------------------------------------------------- the monster switch

// A monster whose stats the test dictates, recording what each spell did.
struct FakeMonster {
    int32_t stats[16] = {0};
    int8_t effects[16] = {0};
    std::vector<int32_t> damageTaken;
    int stores = 0;
    bool undead = false;

    spellcast::Target target() {
        spellcast::Target t;
        t.self = this;
        t.stat = [](void *self, int32_t w) { return static_cast<FakeMonster *>(self)->stats[w]; };
        t.setEffect = [](void *self, int32_t i, int8_t v) {
            static_cast<FakeMonster *>(self)->effects[i] = v;
        };
        t.takeDamage = [](void *self, int32_t n) {
            static_cast<FakeMonster *>(self)->damageTaken.push_back(n);
        };
        t.isUndead = [](void *self) { return static_cast<FakeMonster *>(self)->undead; };
        t.store = [](void *self) { ++static_cast<FakeMonster *>(self)->stores; };
        return t;
    }
};

// The caster side of the monster switch. Ranks are dictated per skill so that
// a branch reading the wrong one is visible rather than merely wrong by the
// same amount everywhere.
struct FakeHooks {
    int32_t ranks[16] = {0};
    int32_t aptitude = 50;
    int8_t timers[32] = {0};
    int16_t damageBonus = 0;
    int meleeSwings = 0;
    // The timer value at the moment the melee swing ran. Spell 14 flags the
    // timer *around* the swing and melee::swing consults it, so the ordering
    // is the behaviour: captured here rather than inferred from the state
    // afterwards, which shows nothing because the flag is cleared again.
    int8_t timerDuringSwing = 99;
    int32_t swingTimerIndex = -1;

    spellcast::MonsterSpellHooks hooks(void *monster) {
        spellcast::MonsterSpellHooks h;
        h.monster = monster;
        h.skillRank = [](void *self, int32_t s) { return static_cast<FakeHooks *>(self)->ranks[s]; };
        h.skillRankWithAttribute = [](void *self, int32_t s) {
            return static_cast<FakeHooks *>(self)->ranks[s];
        };
        h.skillAptitude = [](void *self, int32_t) {
            return static_cast<FakeHooks *>(self)->aptitude;
        };
        h.setSpellTimer = [](void *self, int32_t i, int8_t v) {
            static_cast<FakeHooks *>(self)->timers[i] = v;
        };
        h.setDamageBonus = [](void *self, int16_t b) {
            static_cast<FakeHooks *>(self)->damageBonus = b;
        };
        h.attackMonster = [](void *self, void *) {
            FakeHooks *f = static_cast<FakeHooks *>(self);
            ++f->meleeSwings;
            if (f->swingTimerIndex >= 0) f->timerDuringSwing = f->timers[f->swingTimerIndex];
        };
        return h;
    }
};

// One monster cast with the roll pinned at a plain success.
//
// The shared code hands `caster.self` to every hook, so the caster hooks and
// the monster hooks must agree about what that pointer is. Here it is the
// FakeHooks, and the three Caster hooks are rebound to closures that need no
// state -- wiring the two halves to different objects would silently work on
// this fake and hide a mix-up in the real one.
struct MonsterCast {
    FakeCaster caster;
    FakeMonster monster;
    FakeHooks hooks;

    // `resist` is stat 10 and the evasion cap stat 2. Both default to zero so
    // the advantage gap is the rank, and the roll is decided by aptitude 50
    // against resilience 50.
    // `wantRoll` picks the outcome: 2 a plain success (multiplier 1), 3 a
    // critical (multiplier 2). Anything that scales must be driven at both,
    // because at multiplier 1 a doubled effect and a tripled one are the same
    // number -- which is how the first version of this test let "2x -> 3x"
    // through on two spells.
    void run(int32_t spellId, int32_t school, int32_t magickaCost, int32_t wantRoll = 2) {
        hooks.aptitude = 50;
        monster.stats[9] = 50;
        Rolls r = rollsFor(wantRoll);
        spellcast::Caster c = caster.caster();
        c.self = &hooks;
        c.fatigueMultiplier = [](void *) { return 1; };
        c.hasAilment = [](void *, int32_t) { return false; };
        c.awardSkillXp = [](void *, int32_t, int32_t) {};
        // The rolls-supplied form: the drawn one reaches the game's global RNG,
        // which is a null pointer in a unit test. Rolls 40/30 are a plain
        // success with both chances at 50, so `multiplier` is 1 and each
        // branch below is exercised at its unscaled value.
        spellcast::castAtMonsterWithRolls(c, monster.target(), hooks.hooks(&monster), spellId,
                                          school, magickaCost, r.attack, r.defence);
    }
};

void castMonsterSpell(MonsterCast *mc, int32_t spellId) {
    mc->run(spellId, skills::DESTRUCTION, 0);
}

// Only five branches store. The rest write an effect slot and leave the
// monster unstored, so the effect lands on a record the next decode
// overwrites -- the same shape melee preserves for its spell-7 damage, and
// what both games ship. Asserted as an exact set, so adding a "missing" store
// fails here rather than quietly changing what the monster table holds.
void testWhichBranchesStore() {
    const int32_t stores[] = {4, 11, 12, 13, 15};
    for (int32_t spell = 4; spell <= 20; ++spell) {
        MonsterCast mc;
        mc.monster.undead = true;  // so spell 9 reaches its body
        castMonsterSpell(&mc, spell);
        bool wanted = false;
        for (int32_t s : stores) {
            if (s == spell) wanted = true;
        }
        checkEq(mc.monster.stores, wanted ? 1 : 0,
                "spell " + std::to_string(spell) + (wanted ? " stores" : " does not store"));
    }
}

// Turn undead is the only branch that checks what it is aimed at, and it
// leaves a living monster entirely alone.
void testTurnUndeadGate() {
    MonsterCast living;
    castMonsterSpell(&living, 9);
    checkEq((int32_t)living.monster.damageTaken.size(), 0, "spell 9 spares the living");

    MonsterCast dead;
    dead.monster.undead = true;
    dead.monster.stats[8] = 10;    // armour
    dead.monster.stats[14] = 100;  // maximum health
    castMonsterSpell(&dead, 9);
    checkEq((int32_t)dead.monster.damageTaken.size(), 1, "spell 9 strikes the undead");
    checkEq(dead.monster.damageTaken[0], 50, "60 base less 10 armour, all of a 100 max");
}

// The damage spells share one shape: a base, less armour, floored at four,
// then taken as a percentage of maximum health. A heavily armoured monster is
// what exposes the floor.
void testDamageFloor() {
    MonsterCast mc;
    mc.monster.undead = true;
    mc.monster.stats[8] = 500;
    mc.monster.stats[14] = 100;
    castMonsterSpell(&mc, 9);
    checkEq(mc.monster.damageTaken[0], 4, "damage floors at four before the percentage");
}

// Spell 8 drains the monster and feeds the caster. Its magicka gain is a flat
// twelve that ignores the Conjuration rank the other two scale with -- reading
// that as "all three scale" is the obvious tidy-up and is wrong.
void testDrainFeedsEachVitalSeparately() {
    MonsterCast mc;
    mc.hooks.ranks[skills::CONJURATION] = 5;
    mc.caster.vitals[2] = 10;
    mc.caster.vitals[3] = 100;
    mc.caster.vitals[4] = 10;
    mc.caster.vitals[5] = 100;
    mc.caster.vitals[6] = 10;
    mc.caster.vitals[7] = 100;
    castMonsterSpell(&mc, 8);

    checkEq(mc.monster.damageTaken[0], 12 + 2 * 5, "the drain scales with the Conjuration rank");
    checkEq(mc.caster.vitals[2], 15, "health gains the rank");
    checkEq(mc.caster.vitals[4], 22, "magicka gains a flat twelve, not the rank");
    // Fatigue gains the rank and *then* pays the cast's own five, because
    // `settle` runs after the switch: 10 + 5 - 5. Asserting the gain alone
    // would have been asserting a number the code never holds.
    checkEq(mc.caster.vitals[6], 10, "fatigue gains the rank, then pays the cast");
}

// Each vital is clamped to its own maximum, not to a shared one. The three
// maxima are deliberately different, which is the only arrangement that can
// tell a shared clamp from three separate ones.
void testDrainClampsToEachOwnMaximum() {
    MonsterCast mc;
    mc.hooks.ranks[skills::CONJURATION] = 50;
    mc.caster.vitals[2] = 90;
    mc.caster.vitals[3] = 95;
    mc.caster.vitals[4] = 90;
    mc.caster.vitals[5] = 200;
    mc.caster.vitals[6] = 90;
    mc.caster.vitals[7] = 91;
    castMonsterSpell(&mc, 8);
    checkEq(mc.caster.vitals[2], 95, "health clamps to its own maximum");
    checkEq(mc.caster.vitals[4], 102, "magicka is far from its own and is not clamped");
    // Clamped to 91 by the switch, then charged five by `settle`. The clamp is
    // still what is under test: without it the gain would have carried fatigue
    // to 140 and this would read 135.
    checkEq(mc.caster.vitals[6], 86, "fatigue clamps to its own maximum, then pays the cast");
}

// Spell 16 opens a window a resistant monster can close entirely, and when it
// closes *neither* the timer nor the effect is written: the break is ahead of
// both, not between them.
void testSilenceWindowClosesBeforeBothWrites() {
    {
        MonsterCast open;
        open.run(16, skills::ILLUSION, 0);
        check(open.hooks.timers[15] != 0, "an open window sets the timer");
        checkEq((int32_t)open.monster.effects[6], 1, "and the effect");
    }
    {
        // Resistance *past* the base, so the window is negative rather than
        // zero. That distinction is the whole test: a timer written before the
        // guard would write zero at resistance 10, which reads exactly like
        // not writing at all -- the mutation sweep walked through the first
        // version of this case for that reason. At resistance 25 the window is
        // -15, so a premature write is visible.
        MonsterCast shut;
        shut.monster.stats[2] = 100;  // a high cap, so resistance is what binds
        shut.monster.stats[10] = 25;
        shut.run(16, skills::ILLUSION, 0);
        checkEq((int32_t)shut.hooks.timers[15], 0, "a closed window leaves the timer unset");
        checkEq((int32_t)shut.monster.effects[6], 0, "and the effect untouched");
    }
}

// Spell 14 is a free melee swing with the timer flagged around it. The swing
// reads that timer, so the ordering is the behaviour.
void testMeleeSpellFlagsTheTimerAroundTheSwing() {
    MonsterCast mc;
    mc.hooks.swingTimerIndex = 13;  // spell 14 is timer 13
    castMonsterSpell(&mc, 14);
    checkEq(mc.hooks.meleeSwings, 1, "spell 14 swings once");
    checkEq((int32_t)mc.hooks.timerDuringSwing, -1, "the timer reads -1 during the swing");
    checkEq((int32_t)mc.hooks.timers[13], 0, "and is cleared again afterwards");
}

// The two debuffs are written into a byte and the rank can carry them past it,
// so both cap at 255 before the narrowing.
void testDebuffsCapAt255() {
    const int32_t spells[] = {12, 13};
    for (int32_t spell : spells) {
        MonsterCast mc;
        mc.hooks.ranks[skills::DESTRUCTION] = 1000;
        castMonsterSpell(&mc, spell);
        int32_t slot = spell == 12 ? 4 : 5;
        checkEq((int32_t)mc.monster.effects[slot], (int32_t)(int8_t)255,
                "spell " + std::to_string(spell) + " caps its debuff at 255");
    }
}

// Spell 19 clamps its flee chance to [0, 100]; a resistant monster would
// otherwise drive it negative.
void testFleeChanceClamp() {
    MonsterCast mc;
    mc.monster.stats[2] = 100;
    mc.monster.stats[10] = 30;  // 60 - 150 is well negative
    mc.run(19, skills::ILLUSION, 0);
    checkEq((int32_t)mc.monster.effects[3], 0, "a resisted flee chance clamps to zero, not negative");
}

// Each branch writes its own effect slot and no other. Asserted exhaustively
// because the slots are same-typed neighbours in one array -- the exact hazard
// player_dump exists for, where a swap compiles perfectly and changes the game.
void testEachBranchWritesItsOwnSlot() {
    struct Expect {
        int32_t spell;
        int32_t slot;
    };
    const Expect expect[] = {
        {4, 9}, {7, 1}, {10, 8}, {12, 4}, {13, 5}, {15, 2}, {18, 0},
    };
    for (const Expect &e : expect) {
        MonsterCast mc;
        mc.hooks.ranks[skills::CONJURATION] = 1;
        mc.hooks.ranks[skills::DESTRUCTION] = 1;
        castMonsterSpell(&mc, e.spell);
        check(mc.monster.effects[e.slot] != 0,
              "spell " + std::to_string(e.spell) + " writes slot " + std::to_string(e.slot));
        for (int32_t other = 0; other < 16; ++other) {
            if (other == e.slot) continue;
            checkEq((int32_t)mc.monster.effects[other], 0,
                    "spell " + std::to_string(e.spell) + " leaves slot " + std::to_string(other) +
                        " alone");
        }
    }
}

// The two effects that scale with the multiplier, driven at both. Asserting
// only the unscaled value cannot tell 2x from 3x -- the mutation sweep caught
// exactly that on both of these.
void testScaledEffects() {
    {
        MonsterCast plain;
        plain.run(10, skills::ILLUSION, 0, /*wantRoll=*/2);
        MonsterCast crit;
        crit.run(10, skills::ILLUSION, 0, /*wantRoll=*/3);
        checkEq((int32_t)plain.monster.effects[8], 2, "spell 10 is 2 at a plain success");
        checkEq((int32_t)crit.monster.effects[8], 4, "and 4 at a critical: twice, not three times");
    }
    {
        MonsterCast plain;
        plain.run(18, skills::ILLUSION, 0, /*wantRoll=*/2);
        MonsterCast crit;
        crit.run(18, skills::ILLUSION, 0, /*wantRoll=*/3);
        checkEq((int32_t)plain.monster.effects[0], 3, "spell 18 is 3 at a plain success");
        checkEq((int32_t)crit.monster.effects[0], 6, "and 6 at a critical");
    }
}

// Spell 17 buffs the caster and touches the monster not at all.
void testDamageBonusSpell() {
    MonsterCast mc;
    mc.hooks.ranks[skills::ILLUSION] = 7;
    castMonsterSpell(&mc, 17);
    checkEq(mc.hooks.damageBonus, 17, "spell 17 sets the bonus to ten plus the Illusion rank");
    checkEq(mc.monster.stores, 0, "and does not touch the monster");
}

}  // namespace

int main() {
    testMagickaLadder();
    testPartialFailureRoundsDown();
    testCriticalDoublesTheEffect();
    testFailuresDoNotMultiply();
    testMagickaFloor();
    testExperienceOnSuccessOnly();
    testChanceClamp();
    testFatigueCost();
    testFatigueFloor();
    testNoBleedWithoutCorruption();
    testBleedReadsMaximumHealth();
    testBleedNeverRoundsToZero();
    testBleedIsNotFloored();

    testWhichBranchesStore();
    testTurnUndeadGate();
    testDamageFloor();
    testDrainFeedsEachVitalSeparately();
    testDrainClampsToEachOwnMaximum();
    testSilenceWindowClosesBeforeBothWrites();
    testMeleeSpellFlagsTheTimerAroundTheSwing();
    testDebuffsCapAt255();
    testFleeChanceClamp();
    testEachBranchWritesItsOwnSlot();
    testScaledEffects();
    testDamageBonusSpell();

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("spellcast_unit_test: all checks passed\n");
    return 0;
}
