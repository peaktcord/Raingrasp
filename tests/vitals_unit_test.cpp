// Vitals clamping and spell-timer state, swept rather than sampled.
//
// The clamps are the reason this is worth having. `effectiveVital` pairs three
// current vitals with three maximums in one `int16_t` array, and a cur index
// clamped against the wrong max is **invisible on a fresh character**, because
// cur and max are equal by construction until something damages you. That is
// the same blindness the player fixtures were rebuilt to remove -- see the
// plan's note on 0/1026 indistinguishable pairs -- and a play-through only
// ever visits the pairings it happens to visit.
//
// So every index of the array is swept against every other index's maximum,
// with cur and max deliberately pulled apart, and the two-directional property
// is checked: the right max clamps and every wrong one does not.
//
// The other traps pinned here:
//
//   - **`-4` reads as inactive.** Spell 24 writes it and nothing decodes it,
//     so it falls through to the `> 0` test. Adding a `-4` branch would switch
//     on a spell that has never been on in either shipped game.
//
//   - **`-2` defers to the target, not to the timer.** It is the only
//     sentinel whose answer depends on state outside the array, so it is
//     checked in both directions.
//
//   - **`spellSchool` returns a skill index, not a school id.** Both callers
//     pass it straight to `skillRank`/`skillAptitude`. The values 1, 3, 4, 6,
//     10 are Alteration, Conjuration, Destruction, Illusion and Restoration --
//     the five magic schools in an alphabetical fourteen-skill list whose
//     other nine are weapons and armour. That is the whole explanation for the
//     gaps, and it is why no arithmetic on the spell number reproduces them.
//     Every band boundary is checked, and the five indices are pinned against
//     `skills.hpp` so a renumbering there cannot silently recast every spell.
//
//   - **The fatigue ailment is a mask, not a bit number.** It tests
//     `(ailments & 1) == 1`, and other ailments set higher bits, so every
//     other bit must leave the multiplier alone.
//
// One mutation deliberately is *not* caught, and it is worth naming so the gap
// is not mistaken for an oversight: turning the clamp's `>` into `>=`. At
// `value == maximum` both return the maximum, so the two are equivalent for
// every input. The original writes `>` and so does the port; there is no
// observable difference to pin.

#include <cstdio>
#include <string>

#include "src/common/game/vitals.hpp"

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

SharedArray<int8_t> makeTimers() { return SharedArray<int8_t>(vitals::kSpellCount); }

// ------------------------------------------------------------ spellActive

void testSpellActive() {
    // Every spell, every sentinel: the 1-based indexing is the thing being
    // pinned, and an off-by-one here silently reads a neighbouring spell.
    for (int32_t spell = 1; spell <= vitals::kSpellCount; ++spell) {
        SharedArray<int8_t> timers = makeTimers();

        timers[spell - 1] = vitals::INACTIVE;
        check(!vitals::spellActive(timers, spell, false),
              "spell " + std::to_string(spell) + " inactive at 0");

        timers[spell - 1] = vitals::PERMANENT;
        check(vitals::spellActive(timers, spell, false),
              "spell " + std::to_string(spell) + " active at -1 without a target");

        // -2 is the only sentinel that consults state outside the array.
        timers[spell - 1] = vitals::WHILE_TARGETED;
        check(vitals::spellActive(timers, spell, true),
              "spell " + std::to_string(spell) + " active at -2 with a target");
        check(!vitals::spellActive(timers, spell, false),
              "spell " + std::to_string(spell) + " inactive at -2 without a target");

        // The preserved quirk. -4 is written by spell 24 and never decoded.
        timers[spell - 1] = vitals::UNDECODED;
        check(!vitals::spellActive(timers, spell, true),
              "spell " + std::to_string(spell) + " inactive at -4 even with a target");

        timers[spell - 1] = 1;
        check(vitals::spellActive(timers, spell, false),
              "spell " + std::to_string(spell) + " active at 1 tick");
        timers[spell - 1] = 127;
        check(vitals::spellActive(timers, spell, false),
              "spell " + std::to_string(spell) + " active at 127 ticks");
    }

    // A set spell must not make its neighbours read as set -- the whole array
    // is checked, not just the two adjacent slots.
    for (int32_t spell = 1; spell <= vitals::kSpellCount; ++spell) {
        SharedArray<int8_t> timers = makeTimers();
        timers[spell - 1] = vitals::PERMANENT;
        for (int32_t other = 1; other <= vitals::kSpellCount; ++other) {
            if (other == spell) continue;
            check(!vitals::spellActive(timers, other, true),
                  "spell " + std::to_string(spell) + " does not leak into " +
                      std::to_string(other));
        }
    }

    // Every remaining negative value reads as inactive. -4 is the only one the
    // game writes, but the chain's shape is what is being pinned: anything
    // negative that is not -1 or -2 is off.
    for (int32_t value = -128; value < 0; ++value) {
        if (value == vitals::PERMANENT || value == vitals::WHILE_TARGETED) continue;
        SharedArray<int8_t> timers = makeTimers();
        timers[0] = (int8_t)value;
        check(!vitals::spellActive(timers, 1, false),
              "timer " + std::to_string(value) + " reads inactive without a target");
    }
}

// ------------------------------------------------------------- clearSpell

void testClearSpell() {
    // Clearing must reach the right slot and leave every other alone,
    // including from each of the sentinels rather than only from a tick count.
    const int8_t kStates[] = {1, 127, vitals::PERMANENT, vitals::WHILE_TARGETED,
                             vitals::UNDECODED};
    for (int8_t state : kStates) {
        for (int32_t spell = 1; spell <= vitals::kSpellCount; ++spell) {
            SharedArray<int8_t> timers = makeTimers();
            for (int32_t n = 0; n < vitals::kSpellCount; ++n) timers[n] = state;

            vitals::clearSpell(timers, spell);
            checkEq(timers[spell - 1], vitals::INACTIVE,
                    "clearing spell " + std::to_string(spell) + " zeroes its slot");
            for (int32_t n = 0; n < vitals::kSpellCount; ++n) {
                if (n == spell - 1) continue;
                checkEq(timers[n], state,
                        "clearing spell " + std::to_string(spell) + " leaves slot " +
                            std::to_string(n) + " alone");
            }
        }
    }
}

// ------------------------------------------------------------ spellSchool

void testSpellSchool() {
    // The bands, stated as data so the boundaries are visible rather than
    // implied by a chain of comparisons. The expected value is the *skill*
    // each school casts with -- both callers feed it to `skillRank` -- which
    // is what makes the sequence 1, 3, 4, 6, 10 legible instead of arbitrary.
    struct Band {
        int32_t first, last, skill;
        const char *school;
    };
    const Band kBands[] = {{1, 5, skills::ALTERATION, "Alteration"},
                           {6, 10, skills::CONJURATION, "Conjuration"},
                           {11, 15, skills::DESTRUCTION, "Destruction"},
                           {16, 20, skills::ILLUSION, "Illusion"},
                           {21, 25, skills::RESTORATION, "Restoration"}};

    for (const Band &band : kBands) {
        for (int32_t spell = band.first; spell <= band.last; ++spell) {
            checkEq(vitals::spellSchool(spell), band.skill,
                    "spell " + std::to_string(spell) + " casts with " + band.school);
        }
    }

    // Every band boundary moves, so a band that grew or shrank by one fails
    // rather than being absorbed by a neighbour returning the same skill.
    for (const Band &band : kBands) {
        if (band.last >= vitals::kSpellCount) continue;
        check(vitals::spellSchool(band.last) != vitals::spellSchool(band.last + 1),
              std::string("band boundary after spell ") + std::to_string(band.last));
    }

    // The five are the magic schools of a fourteen-skill list whose other nine
    // are weapons and armour -- which is the entire reason for the gaps. Pinned
    // against `skills.hpp` so a renumbering there cannot silently recast every
    // spell with the wrong skill.
    checkEq(skills::ALTERATION, 1, "Alteration is skill 1");
    checkEq(skills::CONJURATION, 3, "Conjuration is skill 3");
    checkEq(skills::DESTRUCTION, 4, "Destruction is skill 4");
    checkEq(skills::ILLUSION, 6, "Illusion is skill 6");
    checkEq(skills::RESTORATION, 10, "Restoration is skill 10");
}

// ---------------------------------------------------------- effectiveVital

// The three fortified pairs, and nothing else in the array.
struct Pair {
    int32_t cur, max;
    const char *name;
};
const Pair kPairs[] = {{vitals::CUR_HP, vitals::MAX_HP, "health"},
                       {vitals::CUR_MAGICKA, vitals::MAX_MAGICKA, "magicka"},
                       {vitals::CUR_FATIGUE, vitals::MAX_FATIGUE, "fatigue"}};

// Every slot distinct, and every cur well below its max, so a clamp against
// the wrong maximum produces a different number rather than the same one.
SharedArray<int16_t> makeVitals() {
    SharedArray<int16_t> v(vitals::kVitalCount);
    for (int32_t n = 0; n < vitals::kVitalCount; ++n) v[n] = (int16_t)(100 + n * 10);
    // Pull each cur well below its own max.
    v[vitals::CUR_HP] = 10;
    v[vitals::MAX_HP] = 200;
    v[vitals::CUR_MAGICKA] = 20;
    v[vitals::MAX_MAGICKA] = 300;
    v[vitals::CUR_FATIGUE] = 30;
    v[vitals::MAX_FATIGUE] = 400;
    return v;
}

void testEffectiveVitalInactive() {
    // With the spell off, every index is returned untouched -- including the
    // three the spell would otherwise change.
    SharedArray<int16_t> v = makeVitals();
    for (int32_t n = 0; n < vitals::kVitalCount; ++n) {
        checkEq(vitals::effectiveVital(v, n, false, 500), v[n],
                "index " + std::to_string(n) + " untouched with the spell inactive");
    }
}

void testEffectiveVitalUntouchedIndices() {
    // With the spell on, only the three current vitals move. Everything else
    // -- the maximums, level, experience, and the Corruption pair Stormhold's
    // camp NPCs read -- must come back unchanged.
    SharedArray<int16_t> v = makeVitals();
    for (int32_t n = 0; n < vitals::kVitalCount; ++n) {
        bool fortified = false;
        for (const Pair &p : kPairs) fortified = fortified || n == p.cur;
        if (fortified) continue;
        checkEq(vitals::effectiveVital(v, n, true, 7), v[n],
                "index " + std::to_string(n) + " untouched by the fortify spell");
    }
}

void testEffectiveVitalAddsBelowMax() {
    // Below the maximum, the rank is added in full.
    for (const Pair &p : kPairs) {
        SharedArray<int16_t> v = makeVitals();
        for (int32_t rank = 0; rank <= 20; ++rank) {
            checkEq(vitals::effectiveVital(v, p.cur, true, rank), v[p.cur] + rank,
                    std::string(p.name) + " adds rank " + std::to_string(rank) + " below max");
        }
    }
}

void testEffectiveVitalClampsAtItsOwnMax() {
    // At and beyond the maximum, the result is the maximum exactly -- swept
    // across the boundary rather than checked at one point.
    for (const Pair &p : kPairs) {
        SharedArray<int16_t> v = makeVitals();
        int32_t headroom = v[p.max] - v[p.cur];
        for (int32_t over = -2; over <= 5; ++over) {
            int32_t rank = headroom + over;
            if (rank < 0) continue;
            int32_t want = over <= 0 ? v[p.cur] + rank : (int32_t)v[p.max];
            checkEq(vitals::effectiveVital(v, p.cur, true, rank), want,
                    std::string(p.name) + " clamps at its own max, rank offset " +
                        std::to_string(over));
        }
    }
}

// The heart of it: each current vital is clamped against its OWN maximum and
// no other. This is what a swapped index pair breaks, and what a fresh
// character cannot reveal.
void testEffectiveVitalPairing() {
    for (const Pair &p : kPairs) {
        // A rank huge enough to exceed every maximum in the array, so whatever
        // it clamps to *is* the maximum it consulted.
        const int32_t kHugeRank = 10000;
        SharedArray<int16_t> v = makeVitals();
        int32_t got = vitals::effectiveVital(v, p.cur, true, kHugeRank);

        checkEq(got, v[p.max],
                std::string(p.name) + " clamps against its own maximum");

        // And explicitly not against any other slot -- the maximums are all
        // distinct in this fixture, so this is decidable.
        for (int32_t n = 0; n < vitals::kVitalCount; ++n) {
            if (n == p.max) continue;
            check(got != v[n] || v[n] == v[p.max],
                  std::string(p.name) + " does not clamp against index " + std::to_string(n));
        }
    }

    // Moving one pair's maximum must move that pair's result and no other's.
    // This is the two-directional form of the same property, and it is what
    // catches a swap between two pairs rather than a wrong constant.
    for (const Pair &moved : kPairs) {
        SharedArray<int16_t> before = makeVitals();
        SharedArray<int16_t> after = makeVitals();
        after[moved.max] = (int16_t)(after[moved.max] - 5);

        for (const Pair &observed : kPairs) {
            const int32_t kHugeRank = 10000;
            int32_t a = vitals::effectiveVital(before, observed.cur, true, kHugeRank);
            int32_t b = vitals::effectiveVital(after, observed.cur, true, kHugeRank);
            if (observed.cur == moved.cur) {
                checkEq(b, a - 5,
                        std::string("lowering ") + moved.name + " max moves " + observed.name);
            } else {
                checkEq(b, a,
                        std::string("lowering ") + moved.name + " max leaves " + observed.name +
                            " alone");
            }
        }
    }
}

// ------------------------------------------------------- fatigueMultiplier

void testFatigueMultiplier() {
    checkEq(vitals::fatigueMultiplier(0), 1, "no ailments gives 1");
    checkEq(vitals::fatigueMultiplier(1), 3, "the fatigue ailment gives 3");

    // Every other bit must leave it alone: ailments are set with `1 << n`, so
    // higher bits are other ailments and only bit 0 is this one.
    for (int32_t bit = 1; bit < 8; ++bit) {
        int32_t ailments = 1 << bit;
        checkEq(vitals::fatigueMultiplier(ailments), 1,
                "ailment bit " + std::to_string(bit) + " alone does not multiply fatigue");
        checkEq(vitals::fatigueMultiplier(ailments | 1), 3,
                "ailment bit " + std::to_string(bit) + " alongside bit 0 still multiplies");
    }

    // The whole byte, swept: the answer depends on bit 0 and nothing else.
    for (int32_t ailments = 0; ailments < 256; ++ailments) {
        checkEq(vitals::fatigueMultiplier(ailments), (ailments & 1) ? 3 : 1,
                "fatigue multiplier for ailments " + std::to_string(ailments));
    }
}

}  // namespace

int main() {
    testSpellActive();
    testClearSpell();
    testSpellSchool();
    testEffectiveVitalInactive();
    testEffectiveVitalUntouchedIndices();
    testEffectiveVitalAddsBelowMax();
    testEffectiveVitalClampsAtItsOwnMax();
    testEffectiveVitalPairing();
    testFatigueMultiplier();

    if (failures != 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("vitals: all checks passed\n");
    return 0;
}
