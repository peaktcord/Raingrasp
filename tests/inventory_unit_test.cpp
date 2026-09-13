// The inventory, swept on its *post-conditions* rather than its returns.
//
// This is the difference between this test and the ones before it. These
// functions mutate three parallel arrays, and the save format stores all
// three, so a function can return the right `bool` and still leave state that
// corrupts the next save. Every mutation here is therefore checked by
// inspecting the arrays afterwards, and most checks end with a whole-bag
// invariant rather than a single value.
//
// The invariant, stated once and asserted everywhere:
//
//   **`inventory_[i] < 0` iff some `equipped_` slot holds `abs(inventory_[i])`.**
//
// The sign and the slot table are two encodings of one fact. `equip` sets
// both, `unequipItem` clears both, and anything that updates one without the
// other produces an item that is equipped according to combat and unequipped
// according to the UI -- which the save then persists. `checkConsistent()`
// below is that property, and it runs after every mutation in this file.
//
// What else is pinned, and why each is a trap:
//
//   - **Compaction moves both arrays.** `removeItem` shifts `inventory_` and
//     `itemData_` in lockstep. Shifting one alone silently reassigns every
//     later item's charge, which no return value reveals. The fixture gives
//     every slot a distinct charge so a desync is visible.
//
//   - **`removeItem` unequips first.** Removing an equipped item without
//     clearing its slot leaves the slot naming an id that has moved down.
//
//   - **`findEquipped` matches only the equipped copy.** It searches for
//     `-abs(id)`, so an unequipped duplicate of the same item must not match.
//     Two copies of one id is the case that separates a sign match from an
//     `abs` match.
//
//   - **`addItem` truncates the charge to a byte before packing it.** A charge
//     above 127 goes negative and borrows from the quantity field, so 128 does
//     not mean 128. The save stores the packed word, so "fixing" the cast
//     changes what every existing save means.
//
//   - **`skillAt` counts only skills above zero.** It turns a menu row into a
//     skill index, so an off-by-one trains the wrong skill.
//
// Two mutations deliberately are *not* caught, named so the gaps are not
// mistaken for oversights. Both are true equivalents, confirmed by injection:
//
//   - Caching the id across `unequipSlot` in `equip` instead of re-reading the
//     entry. `unequipSlot` can only clear a sign, never change an id, and
//     `-abs()` discards the sign -- so the two agree for every input.
//   - Adding a `break` to `unequipSlot`'s loop. Only one entry per slot can be
//     signed equipped, so every later iteration is already a no-op.

#include <cstdio>
#include <string>

#include "src/common/game/equipment.hpp"
#include "src/common/game/inventory.hpp"
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

// A synthetic item table mirroring the real one's shape: ids 1..10 are the ten
// equippable categories, each mapping to its own slot via the `effect` column,
// and ids 11..20 repeat them so a test can hold two items of one category.
// Ids 21..24 are unequippable (`effect` -1), which is what `isUsable` gates on.
const int32_t kItemCount = 24;

void installItems() {
    Items::count_ = kItemCount;
    Items::items_ = SharedArray<Items::Entry>(kItemCount);
    for (int32_t n = 0; n < kItemCount; ++n) {
        Items::Entry &e = Items::at(n);
        int32_t id = n + 1;
        if (id <= 20) {
            int32_t category = ((id - 1) % 10) + 1;
            e.category = (int8_t)category;
            // The real table's slot mapping: categories 1-4 -> slot 0, 5-6 ->
            // slot 1, then one category per slot up to the shield.
            int32_t slot;
            if (category <= 4) slot = equipment::WEAPON;
            else if (category <= 6) slot = equipment::CUIRASS;
            else slot = category - 5;  // 7->2, 8->3, 9->4, 10->5
            e.effect = (int8_t)slot;
        } else {
            e.category = 11;  // trade goods
            e.effect = (int8_t)equipment::kNotEquippable;
        }
        e.rating = (int8_t)(id * 3);
        e.tier = 1;
    }
}

// A bag with its own storage, so each test starts clean.
struct TestBag {
    SharedArray<int8_t> items;
    SharedArray<int32_t> data;
    SharedArray<int8_t> equipped;
    int8_t count = 0;

    TestBag()
        : items(inventory::kCapacity),
          data(inventory::kCapacity),
          equipped(equipment::kStoredSlotCount) {}

    inventory::Bag view() {
        inventory::Bag b;
        b.items = &items;
        b.data = &data;
        b.equipped = &equipped;
        b.count = &count;
        return b;
    }

    // Charges are distinct per position so a lockstep failure in the
    // compaction is visible rather than coincidentally right.
    void add(int32_t id, int32_t quantity, int32_t charge) {
        inventory::Bag b = view();
        inventory::addItem(b, id, quantity, charge);
    }
};

// The invariant. Runs after every mutation below.
//
// **The encoding cannot distinguish two copies of one item.** `equipped_`
// stores an id, not a position, so with two copies of id 3 in the bag a slot
// holding 3 does not say *which* one is equipped. The invariant is therefore
// stated per id rather than per position:
//
//   - an id with any negatively-signed entry must occupy a slot,
//   - an id occupying a slot must have exactly one negatively-signed entry,
//   - a slot's id must be named by some live entry.
//
// That last clause is what catches a dangling slot after a removal. The
// "exactly one" is what catches two copies both being marked equipped, which
// is the real corruption the ambiguity invites -- the game has no way to
// represent it and the save would round-trip it as something else.
void checkConsistent(TestBag &bag, const std::string &what) {
    for (int32_t id = 1; id <= kItemCount; ++id) {
        int32_t signedEquipped = 0;
        for (int32_t n = 0; n < bag.count; ++n) {
            if (bag.items[n] == (int8_t)-id) ++signedEquipped;
        }

        bool inSlots = false;
        for (int32_t s = 0; s < equipment::kStoredSlotCount; ++s) {
            if (bag.equipped[s] == id) inSlots = true;
        }

        if (signedEquipped > 1) {
            std::printf("FAIL: %s: item %d is signed equipped in %d positions at once\n",
                        what.c_str(), (int)id, (int)signedEquipped);
            ++failures;
        }
        if (signedEquipped > 0 && !inSlots) {
            std::printf("FAIL: %s: item %d is signed equipped but no slot holds it\n",
                        what.c_str(), (int)id);
            ++failures;
        }
        if (signedEquipped == 0 && inSlots) {
            std::printf("FAIL: %s: a slot holds item %d but no entry is signed equipped\n",
                        what.c_str(), (int)id);
            ++failures;
        }
    }

    // And nothing may occupy a slot that no live inventory entry names.
    for (int32_t s = 0; s < equipment::kStoredSlotCount; ++s) {
        if (bag.equipped[s] == 0) continue;
        bool found = false;
        for (int32_t n = 0; n < bag.count; ++n) {
            if (wrappingAbs(bag.items[n]) == bag.equipped[s]) found = true;
        }
        if (!found) {
            std::printf("FAIL: %s: slot %d holds item %d that no entry names\n", what.c_str(),
                        (int)s, (int)bag.equipped[s]);
            ++failures;
        }
    }
}

// ------------------------------------------------------------- the queries

void testHasRoom() {
    // The boundary is the whole content: 23 fits, 24 does not.
    for (int32_t count = 0; count < inventory::kCapacity; ++count) {
        check(inventory::hasRoom(count), "room at " + std::to_string(count));
    }
    check(!inventory::hasRoom(inventory::kCapacity), "no room at capacity");
    check(!inventory::hasRoom(inventory::kCapacity + 1), "no room past capacity");
}

void testAddItemFillsAndStops() {
    TestBag bag;
    for (int32_t n = 0; n < inventory::kCapacity; ++n) {
        inventory::Bag b = bag.view();
        check(inventory::addItem(b, 1 + (n % 10), n, n), "add item " + std::to_string(n));
    }
    checkEq(bag.count, inventory::kCapacity, "bag fills to capacity");

    // A full bag refuses and changes nothing.
    int8_t lastId = bag.items[inventory::kCapacity - 1];
    int32_t lastData = bag.data[inventory::kCapacity - 1];
    inventory::Bag b = bag.view();
    check(!inventory::addItem(b, 5, 99, 99), "a full bag refuses");
    checkEq(bag.count, inventory::kCapacity, "a refused add does not change the count");
    checkEq(bag.items[inventory::kCapacity - 1], lastId, "a refused add does not overwrite");
    checkEq(bag.data[inventory::kCapacity - 1], lastData, "a refused add does not touch data");
}

// `addItem` packs `(quantity << 16) + (int8_t)charge`. The cast is not
// cosmetic: a charge above 127 goes negative and *borrows from the quantity*,
// so 128 does not mean 128. That is the original's arithmetic and the save
// stores the packed word, so changing it changes what every existing save
// means.
void testAddItemPacksCharge() {
    TestBag bag;

    // Below the sign boundary the packing is the obvious one.
    bag.add(1, 5, 100);
    checkEq(bag.data[0], (5 << 16) + 100, "a small charge packs plainly");

    // At and above it, the byte goes negative and borrows.
    bag.add(1, 5, 128);
    checkEq(bag.data[1], (5 << 16) + (int32_t)(int8_t)128, "a charge of 128 borrows");
    check(bag.data[1] != (5 << 16) + 128, "128 is not stored as 128");
    checkEq(bag.data[1], (5 << 16) - 128, "128 borrows exactly one from the quantity field");

    bag.add(1, 5, 255);
    checkEq(bag.data[2], (5 << 16) - 1, "a charge of 255 reads as -1");

    // The boundary itself, swept: 127 is the last value that packs plainly.
    checkEq((int32_t)(int8_t)127, 127, "127 survives the cast");
    checkEq((int32_t)(int8_t)128, -128, "128 does not");
}

void testIsEquipped() {
    TestBag bag;
    bag.add(3, 1, 0);   // equippable
    bag.add(21, 1, 0);  // not equippable

    check(!inventory::isEquipped(bag.items, 0), "a positive entry is not equipped");

    inventory::Bag b = bag.view();
    inventory::equip(b, 0, false);
    check(inventory::isEquipped(bag.items, 0), "a negative entry is equipped");

    // An unequippable item is never equipped whatever its sign -- the
    // usability gate comes first.
    bag.items[1] = (int8_t)-21;
    check(!inventory::isEquipped(bag.items, 1), "an unequippable item is never equipped");
}

void testFindEquipped() {
    TestBag bag;
    bag.add(3, 1, 0);  // position 0, will stay unequipped
    bag.add(3, 1, 0);  // position 1, will be equipped -- same id

    checkEq(inventory::findEquipped(bag.items, bag.count, 3), -1,
            "nothing found when no copy is equipped");

    inventory::Bag b = bag.view();
    inventory::equip(b, 1, false);

    // The point: two copies of one id, and only the equipped one matches.
    // A search on `abs` would return 0 here.
    checkEq(inventory::findEquipped(bag.items, bag.count, 3), 1,
            "finds the equipped copy, not the first copy");

    // Sign-insensitive on the argument, since callers pass either.
    checkEq(inventory::findEquipped(bag.items, bag.count, -3), 1,
            "the search id is taken as absolute");

    // Never looks past the live count.
    bag.count = 1;
    checkEq(inventory::findEquipped(bag.items, bag.count, 3), -1,
            "does not search past the live count");
}

void testSkillAt() {
    SharedArray<SharedArray<int16_t>> skills = makeSharedArray2D<int16_t>(inventory::kSkillCount, 2);

    // Nothing trained: every row is -1.
    for (int32_t n = 0; n < inventory::kSkillCount; ++n) {
        checkEq(inventory::skillAt(skills, n), -1, "no skills gives -1 at " + std::to_string(n));
    }

    // Train a scattered subset; the nth row must be the nth trained skill,
    // which is the off-by-one that would train the wrong one.
    const int32_t kTrained[] = {0, 3, 4, 10, 13};
    const int32_t kTrainedCount = 5;
    for (int32_t n = 0; n < kTrainedCount; ++n) skills[kTrained[n]][0] = 5;

    for (int32_t n = 0; n < kTrainedCount; ++n) {
        checkEq(inventory::skillAt(skills, n), kTrained[n],
                "row " + std::to_string(n) + " is skill " + std::to_string(kTrained[n]));
    }
    checkEq(inventory::skillAt(skills, kTrainedCount), -1, "past the last trained skill is -1");

    // Zero is untrained, not trained-at-zero -- the test is `> 0`.
    skills[1][0] = 0;
    checkEq(inventory::skillAt(skills, 1), kTrained[1], "a zero-rank skill is not counted");
}

// ----------------------------------------------------------- equip/unequip

void testEquipSetsBothEncodings() {
    TestBag bag;
    bag.add(3, 1, 0);  // a weapon, slot 0

    inventory::Bag b = bag.view();
    check(inventory::equip(b, 0, false), "equipping succeeds");

    check(bag.items[0] < 0, "equipping signs the entry negative");
    checkEq(bag.equipped[equipment::WEAPON], 3, "equipping fills the slot");
    checkConsistent(bag, "after equip");

    // Equipping again refuses: the sign already says it is equipped.
    check(!inventory::equip(b, 0, false), "equipping an equipped item refuses");
    check(!inventory::equip(b, 0, true), "even with replace");
    checkConsistent(bag, "after refused re-equip");
}

void testEquipRefusesUnequippable() {
    TestBag bag;
    bag.add(21, 1, 0);  // effect -1

    inventory::Bag b = bag.view();
    check(!inventory::equip(b, 0, false), "an unequippable item cannot be equipped");
    check(bag.items[0] > 0, "a refused equip leaves the sign positive");
    for (int32_t s = 0; s < equipment::kStoredSlotCount; ++s) {
        checkEq(bag.equipped[s], 0, "a refused equip fills no slot");
    }
}

void testEquipOccupiedSlot() {
    // Two different weapons, so they contend for slot 0.
    TestBag bag;
    bag.add(3, 1, 0);
    bag.add(4, 1, 0);

    inventory::Bag b = bag.view();
    inventory::equip(b, 0, false);

    // Without replace: refuses, and leaves the incumbent alone.
    check(!inventory::equip(b, 1, false), "a contested slot refuses without replace");
    checkEq(bag.equipped[equipment::WEAPON], 3, "the incumbent keeps the slot");
    check(bag.items[1] > 0, "the newcomer stays unequipped");
    checkConsistent(bag, "after refused contested equip");

    // With replace: the incumbent is unequipped in *both* encodings.
    check(inventory::equip(b, 1, true), "a contested slot yields with replace");
    checkEq(bag.equipped[equipment::WEAPON], 4, "the newcomer takes the slot");
    check(bag.items[0] > 0, "the incumbent's sign is cleared");
    check(bag.items[1] < 0, "the newcomer's sign is set");
    checkConsistent(bag, "after replacing equip");
}

// `equip` re-reads the entry after `unequipSlot`, because replacing can
// rewrite the very entry being equipped. That happens when the incumbent and
// the newcomer are two copies of the same item.
void testEquipReplacingSameItem() {
    TestBag bag;
    bag.add(3, 1, 0);
    bag.add(3, 1, 0);  // same id, different position

    inventory::Bag b = bag.view();
    inventory::equip(b, 0, false);
    check(inventory::equip(b, 1, true), "replacing with an identical item succeeds");

    checkEq(bag.equipped[equipment::WEAPON], 3, "the slot holds the item");
    check(bag.items[1] < 0, "the newcomer is signed equipped");
    check(bag.items[0] > 0, "the old copy is signed unequipped");
    checkConsistent(bag, "after replacing with the same item");
}

void testUnequipItem() {
    TestBag bag;
    bag.add(3, 1, 0);
    inventory::Bag b = bag.view();
    inventory::equip(b, 0, false);

    inventory::unequipItem(b, 0);
    check(bag.items[0] > 0, "unequipping clears the sign");
    checkEq(bag.equipped[equipment::WEAPON], 0, "unequipping clears the slot");
    checkConsistent(bag, "after unequip");

    // A no-op on something already unequipped.
    inventory::unequipItem(b, 0);
    checkConsistent(bag, "after redundant unequip");

    // **Not tested: an out-of-range index.** `unequipItem` has a bounds check
    // and it is dead -- `isEquipped` runs first and already indexes the array,
    // so an out-of-range call reads out of bounds before the guard it looks
    // protected by. That is the original's ordering, preserved deliberately,
    // and calling it out of range here segfaults rather than exercising
    // anything. All three real callers pass a valid index. See the note in
    // `inventory.cpp` and the entry in `docs/DIVERGENCES.md`.
}

void testUnequipSlot() {
    TestBag bag;
    bag.add(3, 1, 0);  // weapon
    bag.add(7, 1, 0);  // boots, a different slot

    inventory::Bag b = bag.view();
    inventory::equip(b, 0, false);
    inventory::equip(b, 1, false);

    inventory::unequipSlot(b, equipment::WEAPON);
    checkEq(bag.equipped[equipment::WEAPON], 0, "the named slot is cleared");
    check(bag.items[0] > 0, "its item is signed unequipped");

    // The other slot is untouched -- unequipSlot walks the whole inventory, so
    // a wrong comparison would clear everything.
    checkEq(bag.equipped[equipment::BOOTS], 7, "another slot keeps its item");
    check(bag.items[1] < 0, "the other item stays equipped");
    checkConsistent(bag, "after unequipSlot");
}

// --------------------------------------------------------------- removal

void testRemoveItemCompactsBothArrays() {
    TestBag bag;
    // Distinct ids and distinct charges, so a desync between the two arrays is
    // visible rather than coincidentally right.
    for (int32_t n = 0; n < 6; ++n) bag.add(1 + n, 100 + n, n + 1);

    int8_t idsAfter[] = {1, 2, 4, 5, 6};
    int32_t dataAfter[6];
    for (int32_t n = 0; n < 6; ++n) dataAfter[n] = bag.data[n];

    inventory::Bag b = bag.view();
    check(inventory::removeItem(b, 2), "removing the middle item succeeds");
    checkEq(bag.count, 5, "the count drops by one");

    for (int32_t n = 0; n < 5; ++n) {
        checkEq(bag.items[n], idsAfter[n], "compacted id at " + std::to_string(n));
    }
    // The charges must have moved with the ids: position n now holds what
    // position n+1 held, for everything after the hole.
    for (int32_t n = 2; n < 5; ++n) {
        checkEq(bag.data[n], dataAfter[n + 1],
                "compacted charge at " + std::to_string(n) + " moved with its id");
    }
    checkConsistent(bag, "after removing the middle item");
}

void testRemoveItemUnequipsFirst() {
    TestBag bag;
    bag.add(3, 1, 0);  // weapon, will be equipped and removed
    bag.add(7, 1, 0);  // boots, stays

    inventory::Bag b = bag.view();
    inventory::equip(b, 0, false);
    inventory::equip(b, 1, false);

    check(inventory::removeItem(b, 0), "removing an equipped item succeeds");
    checkEq(bag.equipped[equipment::WEAPON], 0, "its slot is cleared, not left dangling");
    checkEq(bag.equipped[equipment::BOOTS], 7, "the other slot survives");
    checkEq(bag.count, 1, "the count drops");
    checkEq(bag.items[0], (int8_t)-7, "the survivor compacted down, still equipped");
    checkConsistent(bag, "after removing an equipped item");
}

void testRemoveItemBounds() {
    TestBag bag;
    bag.add(3, 1, 0);

    inventory::Bag b = bag.view();
    check(!inventory::removeItem(b, 1), "removing past the count refuses");
    check(!inventory::removeItem(b, inventory::kCapacity), "removing past capacity refuses");
    checkEq(bag.count, 1, "a refused removal does not change the count");

    // A negative index reaches here for real, and it used to be an
    // out-of-bounds write that killed the process.
    //
    // `Extension::giveStarFrost` frees a slot by scanning for item 87 or the
    // cheapest unequipped item, then removing what it found -- without
    // checking that it found anything. A full bag whose every item is equipped
    // or worth nothing leaves both candidates at -1 and it calls
    // removeItem(-1). That is the original's own bug (`j.class` l() -> w(n)),
    // but on the JVM the negative index throws, the removal does not happen,
    // and play continues; here it wrote before the array. Refusing reproduces
    // what the Java player observes. Found by the playthrough bot after it had
    // filled its bag from 180 chests.
    check(!inventory::removeItem(b, -1), "removing a negative index refuses");
    checkEq(bag.count, 1, "a negative removal does not change the count");
    check(!inventory::removeItem(b, -100), "a far negative index refuses too");
    checkEq(bag.count, 1, "and still does not change the count");

    check(inventory::removeItem(b, 0), "removing the last item succeeds");
    checkEq(bag.count, 0, "the bag empties");
}

// Removing every item one at a time, from every starting position, must leave
// the bag consistent at each step. This is the sweep the parallel arrays
// deserve: the compaction and the two encodings interact, and a bug in either
// shows up as an inconsistency partway through rather than at the end.
void testRemoveEveryPositionKeepsConsistency() {
    for (int32_t start = 0; start < 8; ++start) {
        TestBag bag;
        for (int32_t n = 0; n < 8; ++n) bag.add(1 + n, 100 + n, n + 1);

        inventory::Bag b = bag.view();
        // Equip what can be: ids 1..8 span several slots.
        for (int32_t n = 0; n < 8; ++n) inventory::equip(b, n, false);
        checkConsistent(bag, "after equipping eight items");

        // Remove from `start` repeatedly until the bag empties.
        int32_t removed = 0;
        while (bag.count > 0) {
            int32_t at = start < bag.count ? start : bag.count - 1;
            check(inventory::removeItem(b, at),
                  "remove at " + std::to_string(at) + " succeeds");
            checkConsistent(bag, "after removal " + std::to_string(removed) + " from start " +
                                     std::to_string(start));
            ++removed;
        }
        checkEq(bag.count, 0, "bag empties from start " + std::to_string(start));
        for (int32_t s = 0; s < equipment::kStoredSlotCount; ++s) {
            checkEq(bag.equipped[s], 0,
                    "every slot is empty once the bag is, from start " + std::to_string(start));
        }
    }
}

}  // namespace

int main() {
    installItems();

    testHasRoom();
    testAddItemFillsAndStops();
    testAddItemPacksCharge();
    testIsEquipped();
    testFindEquipped();
    testSkillAt();
    testEquipSetsBothEncodings();
    testEquipRefusesUnequippable();
    testEquipOccupiedSlot();
    testEquipReplacingSameItem();
    testUnequipItem();
    testUnequipSlot();
    testRemoveItemCompactsBothArrays();
    testRemoveItemUnequipsFirst();
    testRemoveItemBounds();
    testRemoveEveryPositionKeepsConsistency();

    if (failures != 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("inventory: all checks passed\n");
    return 0;
}
