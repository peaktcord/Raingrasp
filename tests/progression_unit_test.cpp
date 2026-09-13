// Skill experience, rank promotion, and the level that follows.
//
// This one exists because the behaviour it covers was, until it was written,
// **not observed by anything**. Disabling Dawnstar's entire inline rank-up
// loop left all nineteen tests in `test_quick` passing, checked by injection.
// The player baselines do record `skills_`, `levelUpMask_` and `leveledUp_`,
// but their levelling fixture writes `skills_[n][2]` directly instead of
// awarding through `awardSkillXp`, so it steps straight over the promotion.
// That is the "exercises without discriminating" trap the plan records twice,
// in a fixture written long before this extraction.
//
// What has to be pinned, and why each is not obvious:
//
//   - **The threshold differs between the games and is a declared
//     divergence.** Dawnstar promotes on `points > 10`, Stormhold on
//     `points >= 10`, so at exactly ten they disagree. Both games award +1 and
//     +2, so ten is reachable in ordinary play. The boundary is swept from
//     both sides under both rule sets rather than sampled, because one `=` is
//     the whole difference.
//
//   - **The level check is unconditional.** It runs whether or not a rank was
//     awarded, in both originals. That is not incidental: item effect 92 adds
//     level XP directly in `useItem`, so the threshold can be crossed on a
//     call that promotes nothing. A `sweep` that only checked after a
//     promotion would lose that, and no baseline would notice.
//
//   - **The mask bit is the attribute halved, not the skill.** `levelUpMask_`
//     is indexed by `skillAttribute_[skill] / 2`, and several skills share an
//     attribute -- Axe, Blunt Weapon and Long Blade are all Strength. So the
//     mapping is many-to-one and a promotion must set the *attribute's* bit,
//     not one per skill. Swept over all fourteen against the real table
//     values.
//
//   - **Level XP carries.** A sweep that banks more than ten reports one
//     level and leaves the remainder in `vitals_[1]`; it does not loop. Both
//     originals check once, after the loop.
//
//   - **`repeating` is unreachable in the shipped game and is pinned anyway.**
//     Dawnstar's `while` can take several ranks from one award. It never does,
//     but not because the awards are small -- `awardSkillXp(13, 8)` exists.
//     It is because Dawnstar sweeps after every award, so the banked total
//     never exceeds ten and ten plus eight is one promotion, never two. That
//     is a property of the call pattern, not the arithmetic, so the flag is
//     tested here with a single large award instead: the `player_dump`
//     baselines cannot reach it, and a mutation flipping Dawnstar's
//     `repeating` to false survives them for exactly that reason. Named in
//     the mutation notes rather than left looking like a gap.

#include <cstdio>
#include <string>

#include "src/common/game/progression.hpp"
#include "src/common/game/skills.hpp"

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

// The real skill -> attribute table, read out of charin.dat and identical in
// both games. Pinned here rather than loaded so the test is sealed: if the
// shipped table ever changes, this disagreeing is the signal.
const int16_t kSkillAttribute[progression::kSkillCount] = {
    0,   // Axe            -> Strength
    4,   // Alteration     -> Willpower
    0,   // Blunt Weapon   -> Strength
    2,   // Conjuration    -> Intelligence
    4,   // Destruction    -> Willpower
    10,  // Heavy Armor    -> Endurance
    12,  // Illusion       -> Personality
    6,   // Light Armor    -> Agility
    0,   // Long Blade     -> Strength
    2,   // Perception     -> Intelligence
    4,   // Restoration    -> Willpower
    2,   // Security       -> Intelligence
    8,   // Short Blade    -> Speed
    12,  // Speechcraft    -> Personality
};

// A player's progression state, freshly zeroed.
struct Fixture {
    SharedArray<SharedArray<int16_t>> skills;
    SharedArray<int16_t> skillAttribute;
    SharedArray<int16_t> vitals;
    int8_t mask = 0;

    Fixture() {
        skills = makeSharedArray2D<int16_t>(progression::kSkillCount, 3);
        skillAttribute = SharedArray<int16_t>(progression::kSkillCount);
        for (int32_t n = 0; n < progression::kSkillCount; ++n) {
            skillAttribute[n] = kSkillAttribute[n];
        }
        vitals = SharedArray<int16_t>(10);
    }

    progression::Progress progress() {
        progression::Progress p;
        p.skills = skills;
        p.skillAttribute = skillAttribute;
        p.vitals = vitals;
        p.levelUpMask = &mask;
        return p;
    }

    int32_t points(int32_t skill) const { return skills[skill][progression::POINTS]; }
    int32_t rank(int32_t skill) const { return skills[skill][progression::RANK]; }
};

progression::Rules dawnstarRules() {
    progression::Rules r;
    r.promoteAtThreshold = false;  // > 10
    r.repeating = true;
    return r;
}

progression::Rules stormholdRules() {
    progression::Rules r;
    r.promoteAtThreshold = true;  // >= 10
    r.repeating = false;
    return r;
}

// ------------------------------------------------------------------ award

void testAward() {
    // Accumulates, on the right skill and nowhere else.
    for (int32_t skill = 0; skill < progression::kSkillCount; ++skill) {
        Fixture f;
        progression::award(f.progress(), skill, 3);
        for (int32_t other = 0; other < progression::kSkillCount; ++other) {
            checkEq(f.points(other), other == skill ? 3 : 0,
                    "award to " + std::to_string(skill) + " touched " + std::to_string(other));
        }
        // Awarding never promotes on its own -- that is sweep's job, and the
        // split is what lets Stormhold bank across a suspended tick.
        checkEq(f.rank(skill), 0, "award promoted skill " + std::to_string(skill));
    }

    // Repeated awards add.
    Fixture f;
    progression::award(f.progress(), skills::AXE, 4);
    progression::award(f.progress(), skills::AXE, 5);
    checkEq(f.points(skills::AXE), 9, "two awards accumulate");

    // Out-of-range indices are ignored at both ends, and write nothing.
    for (int32_t bad : {-1, -5, progression::kSkillCount, progression::kSkillCount + 3}) {
        Fixture g;
        progression::award(g.progress(), bad, 7);
        for (int32_t n = 0; n < progression::kSkillCount; ++n) {
            checkEq(g.points(n), 0, "award to out-of-range " + std::to_string(bad) + " wrote skill " +
                                        std::to_string(n));
        }
    }
}

// -------------------------------------------------------- the threshold

// The declared divergence, swept from both sides. This is the assertion the
// whole module exists to hold still.
void testThresholdDivergence() {
    for (int32_t points = 0; points <= 12; ++points) {
        Fixture d;
        d.skills[skills::AXE][progression::POINTS] = (int16_t)points;
        progression::sweep(d.progress(), dawnstarRules(), skills::AXE, skills::AXE + 1);

        Fixture s;
        s.skills[skills::AXE][progression::POINTS] = (int16_t)points;
        progression::sweep(s.progress(), stormholdRules(), skills::AXE, skills::AXE + 1);

        bool dawnstarPromoted = d.rank(skills::AXE) > 0;
        bool stormholdPromoted = s.rank(skills::AXE) > 0;

        checkEq(dawnstarPromoted ? 1 : 0, points > 10 ? 1 : 0,
                "dawnstar promotes at " + std::to_string(points));
        checkEq(stormholdPromoted ? 1 : 0, points >= 10 ? 1 : 0,
                "stormhold promotes at " + std::to_string(points));

        // The one point where the games disagree, asserted as a difference
        // rather than left implicit in the two lines above. If a future
        // change reconciles them, this fails and says so.
        if (points == 10) {
            check(!dawnstarPromoted && stormholdPromoted,
                  "at exactly 10 points only Stormhold promotes -- "
                  "skill-xp-rank-timing in docs/DIVERGENCES.md");
        } else {
            checkEq(dawnstarPromoted ? 1 : 0, stormholdPromoted ? 1 : 0,
                    "the rules agree at " + std::to_string(points) + " points");
        }
    }
}

// ------------------------------------------------------------- promotion

void testPromotionArithmetic() {
    // Ten points are drained, not zeroed: the remainder carries.
    Fixture f;
    f.skills[skills::LONG_BLADE][progression::POINTS] = 13;
    progression::sweep(f.progress(), stormholdRules(), 0, progression::kSkillCount);
    checkEq(f.points(skills::LONG_BLADE), 3, "the remainder carries after a promotion");
    checkEq(f.rank(skills::LONG_BLADE), 1, "the rank went up by one");
    checkEq(f.vitals[1], 1, "level XP went up by one");

    // Aptitude is not touched by a promotion.
    Fixture g;
    g.skills[skills::AXE][progression::APTITUDE] = 42;
    g.skills[skills::AXE][progression::POINTS] = 11;
    progression::sweep(g.progress(), dawnstarRules(), skills::AXE, skills::AXE + 1);
    checkEq(g.skills[skills::AXE][progression::APTITUDE], 42, "promotion left aptitude alone");
}

// The mask is indexed by the *attribute*, halved -- and attributes are shared
// between skills, so this is many-to-one.
void testMaskBit() {
    for (int32_t skill = 0; skill < progression::kSkillCount; ++skill) {
        Fixture f;
        f.skills[skill][progression::POINTS] = 11;
        progression::sweep(f.progress(), dawnstarRules(), skill, skill + 1);
        int32_t expectedBit = kSkillAttribute[skill] / 2;
        checkEq(f.mask, (int8_t)(1 << expectedBit),
                "skill " + std::to_string(skill) + " sets attribute bit " +
                    std::to_string(expectedBit));
    }

    // Two skills sharing an attribute set the same bit, and the mask ors
    // rather than overwrites. Axe and Long Blade are both Strength.
    check(kSkillAttribute[skills::AXE] == kSkillAttribute[skills::LONG_BLADE],
          "fixture assumption: Axe and Long Blade share an attribute");
    Fixture f;
    f.skills[skills::AXE][progression::POINTS] = 11;
    f.skills[skills::LONG_BLADE][progression::POINTS] = 11;
    f.skills[skills::ILLUSION][progression::POINTS] = 11;
    progression::sweep(f.progress(), dawnstarRules(), 0, progression::kSkillCount);
    int32_t strength = kSkillAttribute[skills::AXE] / 2;
    int32_t personality = kSkillAttribute[skills::ILLUSION] / 2;
    checkEq(f.mask, (int8_t)((1 << strength) | (1 << personality)),
            "two Strength skills and one Personality skill set two bits");
}

// ----------------------------------------------------------- the level

void testLevel() {
    // Ten ranks make a level.
    Fixture f;
    f.vitals[1] = 9;
    f.skills[skills::AXE][progression::POINTS] = 11;
    bool leveled = progression::sweep(f.progress(), dawnstarRules(), skills::AXE, skills::AXE + 1);
    check(leveled, "the tenth rank reports a level-up");
    checkEq(f.vitals[0], 1, "the level went up");

    // Nine do not.
    Fixture g;
    g.vitals[1] = 7;
    g.skills[skills::AXE][progression::POINTS] = 11;
    check(!progression::sweep(g.progress(), dawnstarRules(), skills::AXE, skills::AXE + 1),
          "the eighth rank does not report a level-up");
    checkEq(g.vitals[0], 0, "the level did not go up");

    // **The unconditional check.** No skill qualifies, but level XP is already
    // at the threshold -- as it can be after item effect 92, which adds level
    // XP directly. The level must still go up. A sweep that only checked
    // after promoting would miss this, and no baseline covers it.
    Fixture h;
    h.vitals[1] = 10;
    bool leveledWithoutPromotion =
        progression::sweep(h.progress(), dawnstarRules(), 0, progression::kSkillCount);
    check(leveledWithoutPromotion, "a level-up with no promotion is still reported");
    checkEq(h.vitals[0], 1, "the level went up with no promotion");

    // Level XP carries past ten rather than looping: one level per sweep.
    Fixture i;
    i.vitals[1] = 25;
    check(progression::sweep(i.progress(), stormholdRules(), 0, progression::kSkillCount),
          "a sweep over the threshold reports a level");
    checkEq(i.vitals[0], 1, "only one level per sweep");
    checkEq(i.vitals[1], 25, "the level XP is not drained by the level-up");
}

// ------------------------------------------------------------ repeating

void testRepeating() {
    // Dawnstar's `while` takes as many ranks as the points allow.
    Fixture d;
    d.skills[skills::AXE][progression::POINTS] = 35;
    progression::sweep(d.progress(), dawnstarRules(), skills::AXE, skills::AXE + 1);
    checkEq(d.rank(skills::AXE), 3, "repeating takes three ranks from 35 points");
    checkEq(d.points(skills::AXE), 5, "and leaves the remainder");
    checkEq(d.vitals[1], 3, "each rank adds level XP");

    // Stormhold's `if` takes one and leaves the rest for the next tick.
    Fixture s;
    s.skills[skills::AXE][progression::POINTS] = 35;
    progression::sweep(s.progress(), stormholdRules(), skills::AXE, skills::AXE + 1);
    checkEq(s.rank(skills::AXE), 1, "non-repeating takes one rank from 35 points");
    checkEq(s.points(skills::AXE), 25, "and leaves the rest banked");

    // Repeated sweeps drain it, one per sweep -- which is what "per tick"
    // means for the port that does it this way.
    progression::sweep(s.progress(), stormholdRules(), skills::AXE, skills::AXE + 1);
    checkEq(s.rank(skills::AXE), 2, "the second sweep takes a second rank");
    checkEq(s.points(skills::AXE), 15, "draining ten each time");
}

// ---------------------------------------------------------------- scope

// Dawnstar sweeps the one skill it awarded; Stormhold all fourteen. The range
// is the caller's, so it has to actually bound the work.
void testRange() {
    Fixture f;
    for (int32_t n = 0; n < progression::kSkillCount; ++n) {
        f.skills[n][progression::POINTS] = 11;
    }
    progression::sweep(f.progress(), dawnstarRules(), skills::AXE, skills::AXE + 1);
    for (int32_t n = 0; n < progression::kSkillCount; ++n) {
        checkEq(f.rank(n), n == skills::AXE ? 1 : 0,
                "a single-skill sweep promoted only that skill (" + std::to_string(n) + ")");
    }

    // An empty range promotes nothing but still checks the level, because the
    // check is outside the loop in both originals.
    Fixture g;
    g.vitals[1] = 10;
    check(progression::sweep(g.progress(), dawnstarRules(), 0, 0),
          "an empty range still reports a pending level-up");
}

}  // namespace

int main() {
    testAward();
    testThresholdDivergence();
    testPromotionArithmetic();
    testMaskBit();
    testLevel();
    testRepeating();
    testRange();

    if (failures != 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("progression: all checks passed\n");
    return 0;
}
