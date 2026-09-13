// The shared monster table lookups and save record, swept rather than sampled.
//
// This is the point of the extraction. Before it, `stat`, `name`, `isUndead`
// and the damage clamp could only be exercised through a dungeon walk with the
// real game data present -- so the monster baselines covered whichever types
// and stats that particular walk happened to touch, and nothing said what
// happened at the edges. Here the table is synthetic, so every type and every
// column can be checked, and the test needs no private files.
//
// Three properties are worth stating outright, because each is a real trap:
//
//   - **Type ids are 1-based.** Every lookup subtracts one. An off-by-one gives
//     a wrong monster name or a wrong stat, which reads as a content bug rather
//     than an indexing one, so it is checked across the whole table rather than
//     at one index.
//
//   - **Stats are unsigned.** The table is `int8_t` and several columns exceed
//     127, so a sign-extended read turns a large positive into a large
//     negative. That is checked at exactly the boundary.
//
//   - **The record round-trips.** Both ports serialise the same ten fields plus
//     ten effect bytes. Writing then reading must be the identity, including
//     for negative and boundary values, or saves corrupt in a way that only
//     shows up much later.

#include <cstdio>
#include <string>

#include "src/common/game/monsterdata.hpp"

namespace {

int failures = 0;

void check(bool ok, const std::string &what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    }
}

// A table shaped like the real one: 17 stat columns, as datfiles::loadMonsters
// builds. Values are chosen so every column of every type is distinguishable,
// and so that the >127 range is populated.
monsterdata::TypeTable makeTable(int32_t count) {
    monsterdata::TypeTable table;
    table.count = count;
    table.names = SharedArray<std::string>(count);
    table.stats = makeSharedArray2D<int8_t>(count, 17);
    for (int32_t t = 0; t < count; ++t) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "type%d", t + 1);
        table.names[t] = std::string(buf);
        for (int32_t c = 0; c < 17; ++c) {
            // Spread across the whole byte range, including above 127.
            table.stats[t][c] = (int8_t)((t * 17 + c * 7) & 0xFF);
        }
    }
    return table;
}

// --------------------------------------------------------------- lookups

void testTypeName() {
    const int32_t kCount = 12;
    monsterdata::TypeTable table = makeTable(kCount);

    // Every type, not one: the 1-based convention is the thing being pinned.
    for (int32_t type = 1; type <= kCount; ++type) {
        char want[32];
        std::snprintf(want, sizeof(want), "type%d", type);
        std::string got = monsterdata::typeName(table, type);
        check(got == std::string(want),
              "typeName(" + std::to_string(type) + ") is its own name");
    }

    // The ends, called out separately: an off-by-one shows here first.
    check(monsterdata::typeName(table, 1) == "type1", "first type is index 0");
    check(monsterdata::typeName(table, kCount) == "type" + std::to_string(kCount),
          "last type is the final index");
}

void testTypeStat() {
    const int32_t kCount = 12;
    monsterdata::TypeTable table = makeTable(kCount);

    // Sweep every type against every column.
    for (int32_t type = 1; type <= kCount; ++type) {
        for (int32_t col = 0; col < 17; ++col) {
            int32_t want = ((type - 1) * 17 + col * 7) & 0xFF;
            int32_t got = monsterdata::typeStat(table, type, col);
            check(got == want,
                  "typeStat(" + std::to_string(type) + "," + std::to_string(col) + ")");
        }
    }

    // Unsignedness at the boundary, which is where a sign-extending read
    // diverges: 127 stays 127, 128 must be 128 and not -128.
    monsterdata::TypeTable edge;
    edge.count = 1;
    edge.names = SharedArray<std::string>(1);
    edge.names[0] = std::string("edge");
    edge.stats = makeSharedArray2D<int8_t>(1, 4);
    edge.stats[0][0] = (int8_t)0;
    edge.stats[0][1] = (int8_t)127;
    edge.stats[0][2] = (int8_t)128;
    edge.stats[0][3] = (int8_t)255;
    check(monsterdata::typeStat(edge, 1, 0) == 0, "stat 0 reads as 0");
    check(monsterdata::typeStat(edge, 1, 1) == 127, "stat 127 reads as 127");
    check(monsterdata::typeStat(edge, 1, 2) == 128, "stat 128 reads as 128, not -128");
    check(monsterdata::typeStat(edge, 1, 3) == 255, "stat 255 reads as 255, not -1");
}

void testIsUndead() {
    // Exhaustive over every type id the games can hold, plus the boundaries on
    // both sides. 6, 7 and 8 are undead; nothing else is.
    for (int32_t type = -2; type <= 64; ++type) {
        bool want = (type >= 6 && type <= 8);
        check(monsterdata::isUndeadType(type) == want,
              "isUndeadType(" + std::to_string(type) + ")");
    }
    check(!monsterdata::isUndeadType(5), "5 is not undead");
    check(monsterdata::isUndeadType(6), "6 is undead");
    check(monsterdata::isUndeadType(8), "8 is undead");
    check(!monsterdata::isUndeadType(9), "9 is not undead");
}

void testApplyDamage() {
    // Sweep the whole byte range of hp against a wide range of damage. The
    // property is that hp never goes below zero and never rises.
    for (int32_t hp = 0; hp <= 255; ++hp) {
        for (int32_t dmg = 0; dmg <= 300; dmg += 7) {
            int32_t got = monsterdata::applyDamage(hp, dmg);
            int32_t want = dmg > hp ? 0 : hp - dmg;
            check(got == want,
                  "applyDamage(" + std::to_string(hp) + "," + std::to_string(dmg) + ")");
            check(got >= 0, "applyDamage never goes negative");
            check(got <= hp, "applyDamage never heals");
        }
    }

    check(monsterdata::applyDamage(10, 0) == 10, "zero damage leaves hp alone");
    check(monsterdata::applyDamage(10, 10) == 0, "exact damage reaches zero");
    check(monsterdata::applyDamage(10, 11) == 0, "overkill clamps to zero");
    check(monsterdata::applyDamage(0, 5) == 0, "damage to a dead monster stays zero");
}

// ---------------------------------------------------------- the record

monsterdata::Fields sample(int32_t seed) {
    monsterdata::Fields f;
    f.uid = (int16_t)(seed * 37);
    f.type = (int8_t)(seed % 40 + 1);
    f.hp = (int8_t)(seed * 11);
    f.gridX = (int8_t)(seed % 31);
    f.gridY = (int8_t)(seed % 29);
    f.seen = (seed % 2) == 0;
    f.dungeonId = (int8_t)(seed % 7 + 1);
    f.moveCounter = (int8_t)(seed % 5);
    f.attackPhase = (int8_t)(seed % 3);
    f.lastActionMs = (int64_t)seed * 1000003;
    for (int32_t i = 0; i < monsterdata::kEffectCount; ++i) {
        f.effects[i] = (int8_t)((seed + i * 13) & 0xFF);
    }
    return f;
}

bool same(const monsterdata::Fields &a, const monsterdata::Fields &b) {
    if (a.uid != b.uid || a.type != b.type || a.hp != b.hp) return false;
    if (a.gridX != b.gridX || a.gridY != b.gridY || a.seen != b.seen) return false;
    if (a.dungeonId != b.dungeonId || a.moveCounter != b.moveCounter) return false;
    if (a.attackPhase != b.attackPhase || a.lastActionMs != b.lastActionMs) return false;
    for (int32_t i = 0; i < monsterdata::kEffectCount; ++i) {
        if (a.effects[i] != b.effects[i]) return false;
    }
    return true;
}

monsterdata::Fields roundTrip(const monsterdata::Fields &in) {
    BinaryWriter out;
    monsterdata::writeFields(&out, in);

    BinaryReader reader(out.toByteArray());
    monsterdata::Fields back;
    monsterdata::readFields(&reader, &back);
    return back;
}

void testRecordRoundTrip() {
    for (int32_t seed = 0; seed < 200; ++seed) {
        monsterdata::Fields before = sample(seed);
        check(same(before, roundTrip(before)),
              "record round-trips for seed " + std::to_string(seed));
    }

    // The extremes of every width, which is where a wrong field order or a
    // sign mistake shows up rather than in typical values.
    monsterdata::Fields edge;
    edge.uid = (int16_t)-32768;
    edge.type = (int8_t)-128;
    edge.hp = (int8_t)127;
    edge.gridX = (int8_t)-1;
    edge.gridY = (int8_t)0;
    edge.seen = true;
    edge.dungeonId = (int8_t)127;
    edge.moveCounter = (int8_t)-128;
    edge.attackPhase = (int8_t)-1;
    edge.lastActionMs = (int64_t)-1;
    for (int32_t i = 0; i < monsterdata::kEffectCount; ++i) {
        edge.effects[i] = (int8_t)(i % 2 == 0 ? -128 : 127);
    }
    check(same(edge, roundTrip(edge)), "record round-trips at the width extremes");

    // A boolean must survive as a boolean, both ways.
    monsterdata::Fields seenFalse = sample(3);
    seenFalse.seen = false;
    check(roundTrip(seenFalse).seen == false, "seen=false survives");
    monsterdata::Fields seenTrue = sample(3);
    seenTrue.seen = true;
    check(roundTrip(seenTrue).seen == true, "seen=true survives");
}

void testRecordSize() {
    // The record is a fixed width, and the games' saves depend on it: 2 for the
    // uid, 8 single bytes, 8 for the timestamp, then ten effect bytes.
    BinaryWriter out;
    monsterdata::writeFields(&out, sample(1));
    int32_t expected = 2 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 8 + monsterdata::kEffectCount;
    check(out.toByteArray().length() == expected,
          "record is " + std::to_string(expected) + " bytes");
}

// Field *order* is not implied by a round-trip: swapping two same-width fields
// round-trips perfectly and still breaks every existing save. Pin the order by
// reading the written bytes back positionally.
void testFieldOrder() {
    monsterdata::Fields f;
    f.uid = (int16_t)0x0102;
    f.type = (int8_t)0x03;
    f.hp = (int8_t)0x04;
    f.gridX = (int8_t)0x05;
    f.gridY = (int8_t)0x06;
    f.seen = true;
    f.dungeonId = (int8_t)0x08;
    f.moveCounter = (int8_t)0x09;
    f.attackPhase = (int8_t)0x0A;
    f.lastActionMs = (int64_t)0x1112131415161718LL;
    for (int32_t i = 0; i < monsterdata::kEffectCount; ++i) {
        f.effects[i] = (int8_t)(0x20 + i);
    }

    BinaryWriter out;
    monsterdata::writeFields(&out, f);
    SharedArray<int8_t> raw = out.toByteArray();

    const int32_t want[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x08, 0x09, 0x0A,
                         0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                         0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
    const int32_t wantLen = (int32_t)(sizeof(want) / sizeof(want[0]));
    check(raw.length() == wantLen, "field-order fixture is the expected length");
    for (int32_t i = 0; i < wantLen && i < raw.length(); ++i) {
        check((raw[i] & 0xFF) == want[i],
              "record byte " + std::to_string(i) + " is in the expected position");
    }
}

}  // namespace

int main() {
    testTypeName();
    testTypeStat();
    testIsUndead();
    testApplyDamage();
    testRecordRoundTrip();
    testRecordSize();
    testFieldOrder();

    if (failures != 0) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("monsterdata: all checks passed\n");
    return 0;
}
