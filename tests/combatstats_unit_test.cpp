// The shared combat arithmetic, swept rather than sampled.
//
// This is the point of the extraction. Before it, the attack and defence
// ratings, the weapon damage and the armour rating were reachable only through
// a combat baseline -- which exercises whichever equipment and spell
// combinations a scripted play-through happened to hold, and says nothing
// about the branches it missed. Here the item table is synthetic, so every
// category, every slot and every spell effect is reachable, and the test needs
// no private files.
//
// What it pins, and why each is a real trap:
//
//   - **The slot map.** Slot 1 is the cuirass, not a shield; shields are
//     category 10 and sit in slot 5. Reading the "defence" functions as
//     shield functions is the natural misreading, and it would make the
//     armour weighting below look arbitrary.
//
//   - **The cuirass chain's coincidence.** It reads
//     `category == 5 ? skill 5 : skill 7`, and item category 5 (Heavy Armor)
//     genuinely maps to skill 5 (Heavy Armor) while category 6 maps to skill
//     7. The two tables are unrelated; the agreement at 5 is luck. Both
//     branches are checked so a "tidy-up" that assumes category == skill fails.
//
//   - **The weapon chain's tail.** Categories 1, 2 and 3 are tested for and
//     everything else falls to Short Blade. The whole byte range is swept, so
//     a category that should not reach the tail cannot start doing so quietly.
//
//   - **Two functions disagree on purpose.** Under the bound weapon
//     (effect 14), `attackSkill` returns a Destruction-scaled number while
//     `attackSkillIndex` still reports the equipped weapon's skill, and
//     `attackAptitude` reports the best weapon skill's. The original does
//     this; all three are pinned so nobody reconciles them.
//
//   - **The armour division happens before the bonuses.** Equipment is summed,
//     weighted 4/2/2/1/1, and divided by ten; only then are the spell and
//     potion bonuses added. That makes those bonuses worth roughly ten times
//     their face value, which is the sort of thing a refactor "fixes".
//
//   - **The tenacity divergence stays in the hook.** The rank hook is what
//     Dawnstar's `skillRank` folds `tenacity_` into. A fake rank source proves
//     the shared code asks rather than computes: shifting every rank shifts
//     every rating that depends on one, and nothing else.

#include <cstdio>
#include <string>

#include "src/common/game/combatstats.hpp"
#include "src/common/game/equipment.hpp"
#include "src/common/game/items.hpp"

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

// --------------------------------------------------------------- fixtures

// A synthetic item table. Ids are 1-based; row i has category i and rating
// i * kRatingStep, so an item's id names both its category and its
// contribution and a wrong lookup is legible in the number rather than merely
// wrong.
//
// Ids 1..10 cover the ten equippable categories. Ids 11..20 repeat the
// categories with distinguishable ratings, so a test can put two different
// items of the same category in the same slot.
//
// The step is 7, and both properties of that number are load-bearing:
//
//   - It keeps every rating inside a `int8_t`. At 10 the second run reaches 200
//     and wraps negative, which is a property of the real column too -- every
//     shipped equipment rating fits in 0..127.
//
//   - It makes the weighted slot totals *not* all multiples of ten, which is
//     what lets `testArmourRating` tell summing-then-dividing apart from
//     dividing-then-summing. At a step of 5 every weighted term divides
//     evenly, the two orderings agree, and that assertion silently checks
//     nothing.
// Ids 21..30 mirror 1..10 with a *negative* category byte, which is what the
// `wrappingAbs` in the shared code is for. No shipped row is negative, so nothing
// else here would reach it -- see testNegativeCategories.
const int32_t kItemCount = 30;
const int32_t kRatingStep = 7;
const int32_t kNegativeBase = 20;

void installItems() {
    Items::count_ = kItemCount;
    Items::items_ = SharedArray<Items::Entry>(kItemCount);
    for (int32_t n = 0; n < kItemCount; ++n) {
        Items::Entry &e = Items::at(n);
        int32_t category = (n % 10) + 1;
        e.category = (int8_t)(n >= kNegativeBase ? -category : category);
        e.rating = (int8_t)(((n % kNegativeBase) + 1) * kRatingStep);
        e.tier = 1;
        e.effect = (int8_t)(n % 10);
    }
}

// An id whose category is `category` and whose rating is known.
int32_t itemOfCategory(int32_t category) { return category; }
// The same category, stored negative.
int32_t negativeItemOfCategory(int32_t category) { return kNegativeBase + category; }
int32_t ratingOf(int32_t id) { return ((id - 1) % kNegativeBase + 1) * kRatingStep; }

// A fake rank source. Ranks are `base + skill`, so every skill is
// distinguishable and the "best weapon skill" is decidable by construction;
// `base` stands in for whatever the per-game `skillRank` folds in, tenacity
// included. Aptitudes are `100 + skill` so they can never be confused with a
// rank.
struct FakeSource {
    int32_t base = 0;
    int32_t attributeBonus = 7;  // what `withAttribute` adds, distinctly
    bool effects[32] = {false};
    // Sized as the game stores it, not as the combat code reads it, so a
    // test can put something in the lockpick slot and prove it is ignored.
    int8_t equipped[equipment::kStoredSlotCount] = {0, 0, 0, 0, 0, 0, 0};
    int32_t damageBonus = 0;
    bool potionAttack = false;
    bool potionDefence = false;

    // Lets a test make one skill the winner regardless of index order.
    int32_t boostSkill = -1;
    int32_t boostBy = 0;

    int32_t rankOf(int32_t skill, bool withAttribute) const {
        int32_t r = base + skill + (withAttribute ? attributeBonus : 0);
        if (skill == boostSkill) r += boostBy;
        return r;
    }

    combatstats::Combatant combatant() const {
        combatstats::Combatant c;
        c.equipped = equipped;
        c.self = this;
        c.rank = [](const void *self, int32_t skill, bool withAttribute) {
            return static_cast<const FakeSource *>(self)->rankOf(skill, withAttribute);
        };
        c.aptitude = [](const void *, int32_t skill) { return 100 + skill; };
        c.effect = [](const void *self, int32_t e) {
            return static_cast<const FakeSource *>(self)->effects[e];
        };
        c.damageBonus = damageBonus;
        c.potionAttack = potionAttack;
        c.potionDefence = potionDefence;
        return c;
    }
};

// ------------------------------------------------------- the slot mapping

// The slot map is data, not arithmetic, so it is pinned against the enum
// rather than derived. Getting this wrong is what makes the cuirass read as a
// shield.
void testSlotMap() {
    checkEq(combatstats::WEAPON, 0, "WEAPON slot");
    checkEq(combatstats::CUIRASS, 1, "CUIRASS slot");
    checkEq(combatstats::BOOTS, 2, "BOOTS slot");
    checkEq(combatstats::GLOVES, 3, "GLOVES slot");
    checkEq(combatstats::HELMET, 4, "HELMET slot");
    checkEq(combatstats::SHIELD, 5, "SHIELD slot");
    checkEq(combatstats::kSlotCount, 6, "slot count");

    // The seventh slot is Stormhold's lockpick. It is stored and serialised
    // but read by nothing, so the two counts differ on purpose -- a reader
    // covering seven would walk into it, and a *loader* covering six would
    // corrupt every save from the byte after the shield onward.
    checkEq(equipment::LOCKPICK, 6, "LOCKPICK is the seventh slot");
    checkEq(equipment::kReadableSlotCount, 6, "six slots feed a rating");
    checkEq(equipment::kStoredSlotCount, 7, "seven slots are stored and saved");
    check(equipment::kStoredSlotCount > equipment::kReadableSlotCount,
          "more slots are stored than are read");
}

// The lockpick slot must not reach any rating. Checked with real armour
// present so a contribution would survive the division -- the same fixture
// discipline the weapon slot needed.
void testLockpickSlotIsInert() {
    FakeSource armoured;
    for (int32_t slot = combatstats::CUIRASS; slot < combatstats::kSlotCount; ++slot) {
        armoured.equipped[slot] = (int8_t)(slot + 10);
    }
    FakeSource withPick = armoured;
    withPick.equipped[equipment::LOCKPICK] = (int8_t)11;  // a high-rated row

    checkEq(combatstats::armourRating(withPick.combatant()),
            combatstats::armourRating(armoured.combatant()),
            "an equipped lockpick does not change the armour rating");
    checkEq(combatstats::attackSkill(withPick.combatant(), true),
            combatstats::attackSkill(armoured.combatant(), true),
            "an equipped lockpick does not change the attack rating");
    checkEq(combatstats::defenceSkill(withPick.combatant(), true),
            combatstats::defenceSkill(armoured.combatant(), true),
            "an equipped lockpick does not change the defence rating");
}

// ------------------------------------------------- the category -> skill maps

void testWeaponSkillForCategory() {
    checkEq(combatstats::weaponSkillForCategory(combatstats::CAT_AXE), combatstats::AXE,
            "axe -> Axe");
    checkEq(combatstats::weaponSkillForCategory(combatstats::CAT_BLUNT), combatstats::BLUNT,
            "blunt -> Blunt");
    checkEq(combatstats::weaponSkillForCategory(combatstats::CAT_LONG_SWORD),
            combatstats::LONG_BLADE, "long sword -> Long Blade");
    checkEq(combatstats::weaponSkillForCategory(combatstats::CAT_SHORT_SWORD),
            combatstats::SHORT_BLADE, "short sword -> Short Blade");

    // The tail: everything not tested for lands on Short Blade. Swept across
    // the whole byte range so the boundary cannot drift unnoticed.
    for (int32_t category = -128; category <= 127; ++category) {
        if (category == combatstats::CAT_AXE || category == combatstats::CAT_BLUNT ||
            category == combatstats::CAT_LONG_SWORD) {
            continue;
        }
        checkEq(combatstats::weaponSkillForCategory(category), combatstats::SHORT_BLADE,
                "weapon tail for category " + std::to_string(category));
    }
}

void testCuirassSkillForCategory() {
    // The coincidence worth pinning: category 5 -> skill 5, and it is Heavy
    // Armor on both sides by luck rather than by construction.
    checkEq(combatstats::cuirassSkillForCategory(combatstats::CAT_HEAVY_ARMOR),
            combatstats::HEAVY_ARMOR, "heavy armour cuirass -> Heavy Armor skill");
    checkEq(combatstats::cuirassSkillForCategory(combatstats::CAT_LIGHT_ARMOR),
            combatstats::LIGHT_ARMOR, "light armour cuirass -> Light Armor skill");

    for (int32_t category = -128; category <= 127; ++category) {
        if (category == combatstats::CAT_HEAVY_ARMOR) continue;
        checkEq(combatstats::cuirassSkillForCategory(category), combatstats::LIGHT_ARMOR,
                "cuirass tail for category " + std::to_string(category));
    }
}

// ------------------------------------------------------ bestWeaponSkill

void testBestWeaponSkill() {
    const int32_t kWeaponSkills[] = {combatstats::AXE, combatstats::BLUNT, combatstats::LONG_BLADE,
                                  combatstats::SHORT_BLADE};

    // With ranks rising in skill index, the highest index wins.
    {
        FakeSource s;
        checkEq(combatstats::bestWeaponSkill(s.combatant()), combatstats::SHORT_BLADE,
                "best weapon skill with rising ranks");
    }

    // Each of the four can be made the winner, so none is unreachable.
    for (int32_t n = 0; n < 4; ++n) {
        FakeSource s;
        s.boostSkill = kWeaponSkills[n];
        s.boostBy = 100;
        checkEq(combatstats::bestWeaponSkill(s.combatant()), kWeaponSkills[n],
                "best weapon skill boosted to index " + std::to_string(kWeaponSkills[n]));
    }

    // Ties go to the lower index: every comparison is a strict >. Flattening
    // the ranks makes all four equal, so Axe must win.
    {
        FakeSource s;
        s.base = 0;
        // Cancel the index term by boosting nothing and comparing equal ranks:
        // a source that returns a constant.
        struct Flat {
            static int32_t rank(const void *, int32_t, bool) { return 42; }
            static int32_t apt(const void *, int32_t skill) { return 100 + skill; }
            static bool eff(const void *, int32_t) { return false; }
        };
        int8_t none[combatstats::kSlotCount] = {0, 0, 0, 0, 0, 0};
        combatstats::Combatant c;
        c.equipped = none;
        c.self = nullptr;
        c.rank = &Flat::rank;
        c.aptitude = &Flat::apt;
        c.effect = &Flat::eff;
        checkEq(combatstats::bestWeaponSkill(c), combatstats::AXE,
                "ties go to the lowest weapon skill index");
    }
}

// -------------------------------------------------- the skill index lookups

void testAttackSkillIndex() {
    // Nothing equipped is -1, not a skill.
    {
        FakeSource s;
        checkEq(combatstats::attackSkillIndex(s.combatant()), -1, "attack index unarmed");
    }

    // Every weapon category resolves through the chain.
    for (int32_t category = 1; category <= 4; ++category) {
        FakeSource s;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(category);
        checkEq(combatstats::attackSkillIndex(s.combatant()),
                combatstats::weaponSkillForCategory(category),
                "attack index for weapon category " + std::to_string(category));
    }

    // The spectral weapon overrides the equipped weapon entirely -- including
    // when nothing is equipped, where it gives a skill rather than -1.
    {
        FakeSource s;
        s.effects[combatstats::SPECTRAL] = true;
        checkEq(combatstats::attackSkillIndex(s.combatant()), combatstats::SHORT_BLADE,
                "attack index spectral, unarmed");

        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        checkEq(combatstats::attackSkillIndex(s.combatant()), combatstats::SHORT_BLADE,
                "attack index spectral overrides the equipped axe");
    }

    // The preserved disagreement: the bound weapon changes `attackSkill` but
    // NOT this. An axe still reports Axe.
    {
        FakeSource s;
        s.effects[combatstats::BOUND_WEAPON] = true;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        checkEq(combatstats::attackSkillIndex(s.combatant()), combatstats::AXE,
                "attack index ignores the bound weapon (preserved)");
    }
}

void testDefenceSkillIndex() {
    {
        FakeSource s;
        checkEq(combatstats::defenceSkillIndex(s.combatant()), -1, "defence index with no cuirass");
    }
    {
        FakeSource s;
        s.equipped[combatstats::CUIRASS] = (int8_t)itemOfCategory(combatstats::CAT_HEAVY_ARMOR);
        checkEq(combatstats::defenceSkillIndex(s.combatant()), combatstats::HEAVY_ARMOR,
                "defence index heavy cuirass");
        s.equipped[combatstats::CUIRASS] = (int8_t)itemOfCategory(combatstats::CAT_LIGHT_ARMOR);
        checkEq(combatstats::defenceSkillIndex(s.combatant()), combatstats::LIGHT_ARMOR,
                "defence index light cuirass");
    }

    // A shield in the shield slot must not reach this at all -- it reads slot
    // 1, and slot 5 is where a shield lives.
    {
        FakeSource s;
        s.equipped[combatstats::SHIELD] = (int8_t)itemOfCategory(10);
        checkEq(combatstats::defenceSkillIndex(s.combatant()), -1,
                "a shield does not give a defence skill");
    }
}

// --------------------------------------------------------- attackSkill

void testAttackSkill() {
    // Unarmed is 0, both with and without the attribute bonus -- the chain is
    // only entered with something equipped, so the bonus never applies.
    {
        FakeSource s;
        checkEq(combatstats::attackSkill(s.combatant(), false), 0, "attack unarmed");
        checkEq(combatstats::attackSkill(s.combatant(), true), 0, "attack unarmed, with attribute");
    }

    // Each weapon category, with and without the attribute bonus.
    for (int32_t category = 1; category <= 4; ++category) {
        FakeSource s;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(category);
        int32_t skill = combatstats::weaponSkillForCategory(category);
        checkEq(combatstats::attackSkill(s.combatant(), false), s.rankOf(skill, false),
                "attack for category " + std::to_string(category));
        checkEq(combatstats::attackSkill(s.combatant(), true), s.rankOf(skill, true),
                "attack for category " + std::to_string(category) + " with attribute");
    }

    // The bound weapon replaces everything with 5 + Destruction, taken without
    // the attribute bonus even when one was asked for.
    {
        FakeSource s;
        s.effects[combatstats::BOUND_WEAPON] = true;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        int32_t want = 5 + s.rankOf(combatstats::DESTRUCTION, false);
        checkEq(combatstats::attackSkill(s.combatant(), false), want, "attack bound weapon");
        checkEq(combatstats::attackSkill(s.combatant(), true), want,
                "attack bound weapon ignores the attribute bonus");
    }

    // The spectral weapon uses the best weapon skill, and *does* take the
    // attribute bonus.
    {
        FakeSource s;
        s.effects[combatstats::SPECTRAL] = true;
        checkEq(combatstats::attackSkill(s.combatant(), true),
                s.rankOf(combatstats::SHORT_BLADE, true), "attack spectral, with attribute");
    }

    // Bound beats spectral when both run.
    {
        FakeSource s;
        s.effects[combatstats::BOUND_WEAPON] = true;
        s.effects[combatstats::SPECTRAL] = true;
        checkEq(combatstats::attackSkill(s.combatant(), false),
                5 + s.rankOf(combatstats::DESTRUCTION, false), "bound weapon beats spectral");
    }

    // Fortify adds Alteration -- to the equipped and spectral cases, but not
    // the bound one, which returns before reaching it. That asymmetry is the
    // original's and is pinned in all three.
    {
        FakeSource s;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        int32_t without = combatstats::attackSkill(s.combatant(), false);
        s.effects[combatstats::FORTIFY] = true;
        checkEq(combatstats::attackSkill(s.combatant(), false),
                without + s.rankOf(combatstats::ALTERATION, false), "fortify adds Alteration");
    }
    {
        FakeSource s;
        s.effects[combatstats::SPECTRAL] = true;
        int32_t without = combatstats::attackSkill(s.combatant(), false);
        s.effects[combatstats::FORTIFY] = true;
        checkEq(combatstats::attackSkill(s.combatant(), false),
                without + s.rankOf(combatstats::ALTERATION, false),
                "fortify adds Alteration to the spectral weapon");
    }
    {
        FakeSource s;
        s.effects[combatstats::BOUND_WEAPON] = true;
        int32_t without = combatstats::attackSkill(s.combatant(), false);
        s.effects[combatstats::FORTIFY] = true;
        checkEq(combatstats::attackSkill(s.combatant(), false), without,
                "fortify does NOT reach the bound weapon (preserved)");
    }
}

// -------------------------------------------------------- defenceSkill

void testDefenceSkill() {
    {
        FakeSource s;
        checkEq(combatstats::defenceSkill(s.combatant(), false), 0, "defence with no cuirass");
        checkEq(combatstats::defenceSkill(s.combatant(), true), 0,
                "defence with no cuirass, with attribute");
    }
    for (int32_t category = 5; category <= 6; ++category) {
        FakeSource s;
        s.equipped[combatstats::CUIRASS] = (int8_t)itemOfCategory(category);
        int32_t skill = combatstats::cuirassSkillForCategory(category);
        checkEq(combatstats::defenceSkill(s.combatant(), false), s.rankOf(skill, false),
                "defence for cuirass category " + std::to_string(category));
        checkEq(combatstats::defenceSkill(s.combatant(), true), s.rankOf(skill, true),
                "defence for cuirass category " + std::to_string(category) + " with attribute");
    }
}

// ---------------------------------------------------------- the aptitudes

void testAptitudes() {
    // The unequipped defaults are 20, not 0 -- an unarmed character is not
    // maximally inept, and a zero here would change every combat roll.
    {
        FakeSource s;
        checkEq(combatstats::attackAptitude(s.combatant()), 20, "attack aptitude unarmed");
        checkEq(combatstats::defenceAptitude(s.combatant()), 20, "defence aptitude uncuirassed");
    }

    for (int32_t category = 1; category <= 4; ++category) {
        FakeSource s;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(category);
        checkEq(combatstats::attackAptitude(s.combatant()),
                100 + combatstats::weaponSkillForCategory(category),
                "attack aptitude for category " + std::to_string(category));
    }
    for (int32_t category = 5; category <= 6; ++category) {
        FakeSource s;
        s.equipped[combatstats::CUIRASS] = (int8_t)itemOfCategory(category);
        checkEq(combatstats::defenceAptitude(s.combatant()),
                100 + combatstats::cuirassSkillForCategory(category),
                "defence aptitude for cuirass category " + std::to_string(category));
    }

    // Both magical weapons route the aptitude through the best weapon skill,
    // where `attackSkill` treats them differently. Pinned so nobody aligns them.
    for (int32_t effect : {combatstats::SPECTRAL, combatstats::BOUND_WEAPON}) {
        FakeSource s;
        s.effects[effect] = true;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        checkEq(combatstats::attackAptitude(s.combatant()), 100 + combatstats::SHORT_BLADE,
                "attack aptitude uses the best weapon skill under effect " +
                    std::to_string(effect));
    }
}

// --------------------------------------------------------- weaponDamage

void testWeaponDamage() {
    // Unarmed does no weapon damage at all.
    {
        FakeSource s;
        checkEq(combatstats::weaponDamage(s.combatant()), 0, "weapon damage unarmed");
    }

    // An equipped weapon contributes its rating column, not its category.
    for (int32_t category = 1; category <= 4; ++category) {
        FakeSource s;
        int32_t id = itemOfCategory(category);
        s.equipped[combatstats::WEAPON] = (int8_t)id;
        checkEq(combatstats::weaponDamage(s.combatant()), ratingOf(id),
                "weapon damage is the rating for category " + std::to_string(category));
    }

    // The bound weapon is Destruction-scaled and the spectral one
    // Conjuration-scaled; the constants differ (5 vs 20) and are easy to swap.
    {
        FakeSource s;
        s.effects[combatstats::BOUND_WEAPON] = true;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        checkEq(combatstats::weaponDamage(s.combatant()),
                5 + s.rankOf(combatstats::DESTRUCTION, false), "bound weapon damage");
    }
    {
        FakeSource s;
        s.effects[combatstats::SPECTRAL] = true;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        checkEq(combatstats::weaponDamage(s.combatant()),
                20 + s.rankOf(combatstats::CONJURATION, false), "spectral weapon damage");
    }
    {
        FakeSource s;
        s.effects[combatstats::BOUND_WEAPON] = true;
        s.effects[combatstats::SPECTRAL] = true;
        checkEq(combatstats::weaponDamage(s.combatant()),
                5 + s.rankOf(combatstats::DESTRUCTION, false),
                "bound weapon damage beats spectral");
    }

    // Strength and the attack potion stack onto any of the three bases.
    {
        FakeSource s;
        s.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
        int32_t base = combatstats::weaponDamage(s.combatant());
        s.effects[combatstats::STRENGTH] = true;
        int32_t withStrength = base + 10 + s.rankOf(combatstats::ALTERATION, false);
        checkEq(combatstats::weaponDamage(s.combatant()), withStrength, "strength adds to damage");
        s.potionAttack = true;
        checkEq(combatstats::weaponDamage(s.combatant()), withStrength + 25,
                "the attack potion stacks with strength");
    }

    // The potion applies unarmed too, which is not obvious from the code.
    {
        FakeSource s;
        s.potionAttack = true;
        checkEq(combatstats::weaponDamage(s.combatant()), 25, "the attack potion applies unarmed");
    }
}

// --------------------------------------------------------- armourRating

void testArmourRating() {
    {
        FakeSource s;
        checkEq(combatstats::armourRating(s.combatant()), 0, "armour rating unarmoured");
    }

    // The weights, one slot at a time. The division by ten is what makes these
    // legible: a rating of 10 in the cuirass is 4 * 10 / 10 == 4.
    const int32_t kWeights[combatstats::kSlotCount] = {0, 4, 2, 2, 1, 1};
    for (int32_t slot = combatstats::CUIRASS; slot < combatstats::kSlotCount; ++slot) {
        FakeSource s;
        int32_t id = itemOfCategory(slot + 4);  // any id; only the rating matters
        s.equipped[slot] = (int8_t)id;
        checkEq(combatstats::armourRating(s.combatant()), kWeights[slot] * ratingOf(id) / 10,
                "armour weight for slot " + std::to_string(slot));
    }

    // The weapon slot must not contribute. Two things make this discriminating
    // and both were needed to make an injected `kWeights[WEAPON] = 1` fail:
    //
    //   - It is checked *beside* real armour. With every other slot empty a
    //     weapon's contribution divides to zero, and the assertion holds
    //     whether or not the slot is skipped.
    //
    //   - The weapon is a high-rated one. A low rating disappears into the
    //     same truncation even with armour present.
    {
        FakeSource armoured;
        for (int32_t slot = combatstats::CUIRASS; slot < combatstats::kSlotCount; ++slot) {
            armoured.equipped[slot] = (int8_t)(slot + 10);
        }
        const int32_t kHeavyWeapon = 11;  // Axe category, rating well above the others
        FakeSource armed = armoured;
        armed.equipped[combatstats::WEAPON] = (int8_t)kHeavyWeapon;

        // The fixture only proves anything if counting the weapon *would*
        // change the answer.
        int32_t armouredTotal = 0;
        for (int32_t slot = combatstats::CUIRASS; slot < combatstats::kSlotCount; ++slot) {
            armouredTotal += kWeights[slot] * ratingOf(slot + 10);
        }
        check(armouredTotal / 10 != (armouredTotal + ratingOf(kHeavyWeapon)) / 10,
              "the weapon-slot fixture would notice the weapon being counted");

        checkEq(combatstats::armourRating(armed.combatant()),
                combatstats::armourRating(armoured.combatant()),
                "the weapon slot does not contribute to armour");
    }

    // All five slots together, summed and then divided -- not divided and then
    // summed, which would truncate five times instead of once.
    {
        FakeSource s;
        int32_t summedThenDivided = 0;
        int32_t dividedThenSummed = 0;
        for (int32_t slot = combatstats::CUIRASS; slot < combatstats::kSlotCount; ++slot) {
            int32_t id = slot + 10;  // ids 11..15, distinct ratings, none a multiple of 10
            s.equipped[slot] = (int8_t)id;
            summedThenDivided += kWeights[slot] * ratingOf(id);
            dividedThenSummed += kWeights[slot] * ratingOf(id) / 10;
        }
        summedThenDivided /= 10;

        // The fixture only proves anything if the two orderings actually
        // differ on it -- at a rating step that divides evenly they agree, and
        // the assertion below would hold either way. See kRatingStep.
        check(summedThenDivided != dividedThenSummed,
              "the armour fixture distinguishes the two division orders");
        checkEq(combatstats::armourRating(s.combatant()), summedThenDivided,
                "all five armour slots sum before dividing");
    }

    // The bonuses land after the division, so they are worth ten times what
    // the same number of rating points would be. Checked against a fixture
    // whose equipment contributes a known amount.
    {
        FakeSource s;
        s.equipped[combatstats::CUIRASS] = (int8_t)itemOfCategory(combatstats::CAT_HEAVY_ARMOR);
        int32_t base = combatstats::armourRating(s.combatant());

        s.effects[combatstats::SHIELD_SPELL] = true;
        int32_t withSpell = base + 10 + s.rankOf(combatstats::ALTERATION, false);
        checkEq(combatstats::armourRating(s.combatant()), withSpell,
                "the shield spell adds after the division");

        s.damageBonus = 33;
        s.effects[combatstats::BOUND_ARMOR] = true;
        checkEq(combatstats::armourRating(s.combatant()), withSpell + 33, "bound armour adds the bonus");

        s.potionDefence = true;
        checkEq(combatstats::armourRating(s.combatant()), withSpell + 33 + 15,
                "the defence potion stacks with both");
    }

    // Bound armour with a zero bonus adds nothing, so the effect flag alone is
    // not what carries the value.
    {
        FakeSource s;
        s.effects[combatstats::BOUND_ARMOR] = true;
        s.damageBonus = 0;
        checkEq(combatstats::armourRating(s.combatant()), 0, "bound armour with no bonus is inert");
    }
}

// ------------------------------------------------- the negative categories

// The category is read through `wrappingAbs`. No shipped row is negative, so this is
// the only thing that exercises it -- and without a case here, dropping the
// `wrappingAbs` passes every other check in this file while sending every negative
// row into the Short Blade and Light Armor tails.
//
// A modded or corrupt table is the only way to reach this, which is precisely
// why the behaviour should be stated rather than left to whichever way the
// sign happens to fall.
void testNegativeCategories() {
    for (int32_t category = 1; category <= 4; ++category) {
        FakeSource positive;
        positive.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(category);
        FakeSource negative;
        negative.equipped[combatstats::WEAPON] = (int8_t)negativeItemOfCategory(category);

        checkEq(combatstats::attackSkillIndex(negative.combatant()),
                combatstats::attackSkillIndex(positive.combatant()),
                "a negative weapon category resolves as its absolute value, category " +
                    std::to_string(category));
        checkEq(combatstats::attackSkill(negative.combatant(), true),
                combatstats::attackSkill(positive.combatant(), true),
                "negative weapon category gives the same attack rating, category " +
                    std::to_string(category));
    }

    for (int32_t category = 5; category <= 6; ++category) {
        FakeSource positive;
        positive.equipped[combatstats::CUIRASS] = (int8_t)itemOfCategory(category);
        FakeSource negative;
        negative.equipped[combatstats::CUIRASS] = (int8_t)negativeItemOfCategory(category);

        checkEq(combatstats::defenceSkillIndex(negative.combatant()),
                combatstats::defenceSkillIndex(positive.combatant()),
                "a negative cuirass category resolves as its absolute value, category " +
                    std::to_string(category));
    }

    // Specifically: the heavy cuirass stored negative must still be Heavy
    // Armor, not fall to the Light tail. That is the case dropping `wrappingAbs`
    // breaks, and it is the one a mod would hit first.
    {
        FakeSource s;
        s.equipped[combatstats::CUIRASS] =
            (int8_t)negativeItemOfCategory(combatstats::CAT_HEAVY_ARMOR);
        checkEq(combatstats::defenceSkillIndex(s.combatant()), combatstats::HEAVY_ARMOR,
                "a negative heavy cuirass does not fall to the Light Armor tail");
    }
}

// ------------------------------------------------- the divergence hook

// The whole reason the shared code takes a rank hook: Dawnstar's `skillRank`
// adds +4 when `tenacity_` is set. Shifting every rank by a constant must move
// every rating that consults one, and nothing else -- which is what proves the
// shared code asks for a rank rather than computing one, and therefore that
// the divergence cannot leak into this file.
void testRankHookCarriesTheDivergence() {
    const int32_t kTenacity = 4;

    FakeSource plain;
    plain.equipped[combatstats::WEAPON] = (int8_t)itemOfCategory(combatstats::CAT_AXE);
    plain.equipped[combatstats::CUIRASS] = (int8_t)itemOfCategory(combatstats::CAT_HEAVY_ARMOR);

    FakeSource tenacious = plain;
    tenacious.base += kTenacity;

    // Ratings that consult a rank shift by exactly the bonus.
    checkEq(combatstats::attackSkill(tenacious.combatant(), false),
            combatstats::attackSkill(plain.combatant(), false) + kTenacity,
            "tenacity shifts the attack rating");
    checkEq(combatstats::defenceSkill(tenacious.combatant(), false),
            combatstats::defenceSkill(plain.combatant(), false) + kTenacity,
            "tenacity shifts the defence rating");

    // Ratings that do not consult a rank are untouched. Weapon damage from an
    // equipped weapon is its rating column, so tenacity must not reach it.
    checkEq(combatstats::weaponDamage(tenacious.combatant()),
            combatstats::weaponDamage(plain.combatant()),
            "tenacity does not reach equipped weapon damage");
    checkEq(combatstats::armourRating(tenacious.combatant()),
            combatstats::armourRating(plain.combatant()),
            "tenacity does not reach the armour rating without a spell");

    // Aptitudes come from a separate hook and must not move either.
    checkEq(combatstats::attackAptitude(tenacious.combatant()),
            combatstats::attackAptitude(plain.combatant()),
            "tenacity does not reach the attack aptitude");

    // A uniform shift cannot change *which* skill is best, so the indices are
    // stable under it -- otherwise the two games would pick different weapons.
    checkEq(combatstats::bestWeaponSkill(tenacious.combatant()),
            combatstats::bestWeaponSkill(plain.combatant()),
            "tenacity does not change the best weapon skill");
    checkEq(combatstats::attackSkillIndex(tenacious.combatant()),
            combatstats::attackSkillIndex(plain.combatant()),
            "tenacity does not change the attack skill index");
}

}  // namespace

int main() {
    installItems();

    testSlotMap();
    testLockpickSlotIsInert();
    testWeaponSkillForCategory();
    testCuirassSkillForCategory();
    testBestWeaponSkill();
    testAttackSkillIndex();
    testDefenceSkillIndex();
    testAttackSkill();
    testDefenceSkill();
    testAptitudes();
    testWeaponDamage();
    testArmourRating();
    testNegativeCategories();
    testRankHookCarriesTheDivergence();

    if (failures != 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("combatstats: all checks passed\n");
    return 0;
}
