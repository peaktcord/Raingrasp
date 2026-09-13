// The batched small helpers, swept where the domain is small enough to sweep.
//
// Most of these are three lines and their tests are correspondingly short. Two
// are not, and they are why this file exists:
//
//   - **`isWalkable` is a three-bit predicate, so its domain is 256 values and
//     is swept exhaustively.** The bits are wall (0), closed door (1) and
//     blocked (5), and bit 5 is the one a reader misses -- 0x20 does not look
//     like a neighbour of 1 and 2. A sweep over the whole byte is cheap and
//     catches both a dropped test and a wrong mask, including the sign
//     behaviour of `int8_t` at the top of the range.
//
//   - **`describeSpell`'s newlines are load-bearing.** The caller splits the
//     result on them to lay out the spell info screen, so the count and
//     placement are part of the contract, not formatting. Three newlines, four
//     fields, in a fixed order.
//
// `readNpcMessages` is checked for the thing it exists to do: it takes both a
// count from the file and a count from the caller, and throwing on a mismatch
// is the whole point. A silent accept there would mean the data and the code
// had drifted with nothing to say so.
//
// Two mutations deliberately are *not* caught, named so the gaps are not
// mistaken for oversights. Both are true equivalents, confirmed by injection:
//
//   - Swapping `isWalkable`'s WALL and MONSTER tests. The function is a
//     conjunction of three independent bit tests, so their order cannot
//     matter; only dropping one or changing a mask can.
//   - Reading `expected` rather than `count` in `readNpcMessages`'s loop. The
//     throw above guarantees the two are equal by the time the loop runs.

#include <cstdio>
#include <string>
#include <vector>

#include "src/common/game/skills.hpp"
#include "src/common/game/smallhelpers.hpp"
#include "src/common/game/spells.hpp"

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

void checkStr(const std::string &got, const std::string &want, const std::string &what) {
    std::string g = got;
    if (g != want) {
        std::printf("FAIL: %s:\n  got  %s\n  want %s\n", what.c_str(), g.c_str(), want.c_str());
        ++failures;
    }
}

// ------------------------------------------------------------- isWalkable

// The whole byte, because the domain is small enough that sampling would be a
// choice and sweeping is not.
void testIsWalkable() {
    for (int32_t value = 0; value < 256; ++value) {
        int8_t tile = (int8_t)value;
        bool wall = (value & 1) != 0;
        bool door = (value & 2) != 0;
        bool blocked = (value & 0x20) != 0;
        bool want = !wall && !door && !blocked;

        check(smallhelpers::isWalkable(tile) == want,
              "isWalkable(" + std::to_string(value) + ") should be " +
                  (want ? "true" : "false"));
    }

    // The three bits named, so a failure says which one moved rather than just
    // which byte.
    check(smallhelpers::isWalkable(0), "an empty tile is walkable");
    check(!smallhelpers::isWalkable((int8_t)smallhelpers::WALL), "a wall is not walkable");
    check(!smallhelpers::isWalkable((int8_t)smallhelpers::MONSTER),
          "a closed door is not walkable");
    check(!smallhelpers::isWalkable((int8_t)smallhelpers::CHAMPION), "a champion tile is not walkable");

    // Every *other* bit must be ignored. This is what catches a mask widened
    // by accident -- bits 2, 3, 4, 6 and 7 carry unrelated flags.
    const int32_t kIgnored[] = {4, 8, 16, 64, 128};
    for (int32_t bit : kIgnored) {
        check(smallhelpers::isWalkable((int8_t)bit),
              "bit " + std::to_string(bit) + " alone does not block walking");
    }

    // And the bits stay independent when combined with an ignored one.
    for (int32_t bit : kIgnored) {
        check(!smallhelpers::isWalkable((int8_t)(bit | smallhelpers::CHAMPION)),
              "bit " + std::to_string(bit) + " does not unblock a blocked tile");
    }
}

// ---------------------------------------------------------------- saveSize

void testSaveSize() {
    checkEq(smallhelpers::saveSize(true), 400, "a full save is 400 bytes");
    checkEq(smallhelpers::saveSize(false), 200, "a partial save is 200 bytes");
    check(smallhelpers::saveSize(true) > smallhelpers::saveSize(false),
          "the full buffer is the larger of the two");
}

// ----------------------------------------------------------- describeSpell

// A spell table built by the test, so the format is checked without the JARs.
void installSpells() {
    Spell::all_ = SharedArray<Spell *>(3);
    for (int32_t n = 0; n < 3; ++n) {
        Spell *s = new Spell();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Spell%d", n);
        s->name_ = std::string(buf);
        s->skill_ = (int8_t)(n == 0 ? skills::ALTERATION
                                   : (n == 1 ? skills::DESTRUCTION : skills::RESTORATION));
        s->magickaCost_ = (int8_t)(5 + n * 10);
        std::snprintf(buf, sizeof(buf), "Does thing %d.", n);
        s->description_ = std::string(buf);
        Spell::all_[n] = s;
    }
}

SharedArray<std::string> makeSkillNames() {
    SharedArray<std::string> names(skills::kSkillCount);
    const char *kNames[] = {"Axe",         "Alteration", "Blunt Weapon", "Conjuration",
                            "Destruction", "Heavy Armor", "Illusion",    "Light Armor",
                            "Long Blade",  "Perception", "Restoration",  "Security",
                            "Short Blade", "Speechcraft"};
    for (int32_t n = 0; n < skills::kSkillCount; ++n) names[n] = std::string(kNames[n]);
    return names;
}

void testDescribeSpell() {
    SharedArray<std::string> skillNames = makeSkillNames();

    // The exact string, including where the newlines fall. The caller splits
    // on them, so this is a contract rather than cosmetics.
    checkStr(smallhelpers::describeSpell(skillNames, 0),
             "Spell0\nAlteration\nCost: 5\nDoes thing 0.", "the first spell's description");
    checkStr(smallhelpers::describeSpell(skillNames, 1),
             "Spell1\nDestruction\nCost: 15\nDoes thing 1.", "the second spell's description");
    checkStr(smallhelpers::describeSpell(skillNames, 2),
             "Spell2\nRestoration\nCost: 25\nDoes thing 2.", "the third spell's description");

    // Exactly three newlines: four fields, one separator between each. A
    // fourth would add a blank line to the screen; a missing one would run two
    // fields together.
    for (int32_t spell = 0; spell < 3; ++spell) {
        std::string s = smallhelpers::describeSpell(skillNames, spell);
        int32_t newlines = 0;
        for (char c : s) {
            if (c == '\n') ++newlines;
        }
        checkEq(newlines, 3, "spell " + std::to_string(spell) + " has three newlines");
    }

    // The skill name comes from the table by index, so a spell's skill number
    // must select the matching row -- this is what breaks if the skill enum
    // and the shipped table ever disagree.
    checkStr(smallhelpers::describeSpell(skillNames, 1).substr(7, 11), "Destruction",
             "the skill name is looked up by the spell's skill index");
}

// ------------------------------------------------- the string table readers

// A BinaryReader over bytes the test lays out, so both readers are checked
// against a known encoding rather than against a fixture file.
BinaryReader *streamOf(const std::vector<int8_t> &bytes) {
    SharedArray<int8_t> arr((int32_t)bytes.size());
    for (int32_t n = 0; n < (int32_t)bytes.size(); ++n) arr[n] = bytes[n];
    return new BinaryReader(arr);
}

void putShort(std::vector<int8_t> &out, int32_t v) {
    out.push_back((int8_t)((v >> 8) & 0xFF));
    out.push_back((int8_t)(v & 0xFF));
}

void putInt(std::vector<int8_t> &out, int32_t v) {
    out.push_back((int8_t)((v >> 24) & 0xFF));
    out.push_back((int8_t)((v >> 16) & 0xFF));
    out.push_back((int8_t)((v >> 8) & 0xFF));
    out.push_back((int8_t)(v & 0xFF));
}

void putUtf(std::vector<int8_t> &out, const std::string &s) {
    putShort(out, (int32_t)s.size());
    for (char c : s) out.push_back((int8_t)c);
}

void testReadStringTable() {
    // A short count, then that many UTF strings.
    std::vector<int8_t> bytes;
    putShort(bytes, 3);
    putUtf(bytes, "alpha");
    putUtf(bytes, "beta");
    putUtf(bytes, "");  // empty strings are legal and must round-trip

    SharedArray<std::string> table = smallhelpers::readStringTable(streamOf(bytes));
    checkEq(table.length(), 3, "the table has the stated length");
    checkStr(table[0], "alpha", "first string");
    checkStr(table[1], "beta", "second string");
    checkStr(table[2], "", "an empty string survives");

    // A zero count is a valid empty table, not an error.
    std::vector<int8_t> empty;
    putShort(empty, 0);
    checkEq(smallhelpers::readStringTable(streamOf(empty)).length(), 0, "a zero count is legal");
}

void testReadNpcMessages() {
    // The happy path: the file's count and the caller's expectation agree.
    std::vector<int8_t> bytes;
    putInt(bytes, 2);  // an int here, not a short -- a different table format
    putUtf(bytes, "hello");
    putUtf(bytes, "goodbye");

    SharedArray<std::string> msgs = smallhelpers::readNpcMessages(3, 2, streamOf(bytes));
    checkEq(msgs.length(), 2, "both messages are read");
    checkStr(msgs[0], "hello", "first message");
    checkStr(msgs[1], "goodbye", "second message");

    // The mismatch is the point: expecting a different count must throw rather
    // than read on. Checked in both directions, since expecting *fewer* is the
    // case that would otherwise silently truncate.
    for (int32_t expected : {1, 3}) {
        std::vector<int8_t> b2;
        putInt(b2, 2);
        putUtf(b2, "hello");
        putUtf(b2, "goodbye");

        bool threw = false;
        try {
            smallhelpers::readNpcMessages(3, expected, streamOf(b2));
        } catch (const std::runtime_error &) {
            threw = true;
        }
        check(threw, "expecting " + std::to_string(expected) + " where the file says 2 throws");
    }
}

}  // namespace

int main() {
    installSpells();

    testIsWalkable();
    testSaveSize();
    testDescribeSpell();
    testReadStringTable();
    testReadNpcMessages();

    if (failures != 0) {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("smallhelpers: all checks passed\n");
    return 0;
}
