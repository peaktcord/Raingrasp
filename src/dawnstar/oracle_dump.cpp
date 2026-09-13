#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "src/common/platform/desktop.hpp"
#include "src/common/game/util.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/dawnstar/dungeon_gen.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/dawnstar/profile.hpp"
#include "src/dawnstar/variant.hpp"
#include "src/dawnstar/npc_script.hpp"
#include "src/common/game/player.hpp"

using namespace dawnstar;

static std::string g_out;

static void row(const std::string &line) {
    g_out += line;
    g_out += "\n";
}

static std::string i2s(int32_t v) { return std::to_string(v); }

static std::string longHex(int64_t v) {
    uint64_t u = (uint64_t)v;
    if (u == 0) return "0";
    char buf[17];
    int n1 = 0;
    while (u != 0) {
        int d = (int)(u & 0xF);
        buf[n1++] = (char)(d < 10 ? ('0' + d) : ('a' + d - 10));
        u >>= 4;
    }
    std::string s1;
    for (int k2 = n1 - 1; k2 >= 0; --k2) s1 += buf[k2];
    return s1;
}

static std::string joinInts(const std::vector<int32_t> &v) {
    std::string s1;
    for (size_t n1 = 0; n1 < v.size(); ++n1) {
        if (n1) s1 += ",";
        s1 += std::to_string(v[n1]);
    }
    return s1;
}

template <typename T>
static void rowInts(const std::string &tag, const T *data, int32_t len) {
    std::vector<int32_t> v;
    for (int32_t n = 0; n < len; ++n) v.push_back((int32_t)data[n]);
    row(tag + "\t" + i2s(len) + "\t" + joinInts(v));
}

static void rowIntsArr(const std::string &tag, const SharedArray<int8_t> &a) {
    std::vector<int32_t> v;
    for (int32_t n1 = 0; n1 < a.length(); ++n1) v.push_back((int32_t)a[n1]);
    row(tag + "\t" + i2s(a.length()) + "\t" + joinInts(v));
}

static void rowStrings(const std::string &tag, const SharedArray<std::string> &a) {
    std::string s1;
    for (int32_t n1 = 0; n1 < a.length(); ++n1) {
        if (n1) s1 += "|";
        s1 += a[n1];
    }
    row(tag + "\t" + i2s(a.length()) + "\t" + s1);
}

static int emit(const char *mode, const char *path) {
    if (std::string(mode) == "--write") {
        std::FILE *fp = std::fopen(path, "wb");
        if (fp == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", path);
            return 2;
        }
        std::fwrite(g_out.data(), 1, g_out.size(), fp);
        std::fclose(fp);
        std::printf("wrote %s\n", path);
        return 0;
    }
    std::FILE *fp = std::fopen(path, "rb");
    if (fp == nullptr) {
        std::fprintf(stderr, "cannot read baseline %s\n", path);
        return 2;
    }
    std::string expected;
    char buf[4096];
    size_t n1;
    while ((n1 = std::fread(buf, 1, sizeof(buf), fp)) > 0) expected.append(buf, n1);
    std::fclose(fp);

    std::string got = g_out;
    got.erase(std::remove(got.begin(), got.end(), '\r'), got.end());
    expected.erase(std::remove(expected.begin(), expected.end(), '\r'), expected.end());
    if (got == expected) {
        size_t rows = 0;
        for (char c1 : got) {
            if (c1 == '\n') ++rows;
        }
        std::printf("pure surface matches the original: %zu rows\n", rows);
        return 0;
    }
    std::vector<std::string> a, b;
    for (std::string *src : {&got, &expected}) {
        std::vector<std::string> &out = (src == &got) ? a : b;
        std::string line;
        for (char c2 : *src) {
            if (c2 == '\n') { out.push_back(line); line.clear(); } else { line += c2; }
        }
        if (!line.empty()) out.push_back(line);
    }
    std::printf("FAIL: this no longer matches the original bytecode\n");
    int shown = 0;
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const std::string &ga = i < a.size() ? a[i] : std::string();
        const std::string &gb = i < b.size() ? b[i] : std::string();
        if (ga != gb) {
            if (shown == 0) std::printf("  first difference at line %zu\n", i + 1);
            if (shown < 5) {
                std::printf("    original: %s\n", gb.c_str());
                std::printf("    this port: %s\n", ga.c_str());
            }
            ++shown;
        }
    }
    std::printf("  %d row(s) differ; %zu expected, %zu produced\n", shown, b.size(),
                a.size());
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: %s <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }
    const char *mode = "--write";
    const char *path = argv[2];
    if (argc >= 4) {
        mode = argv[2];
        path = argv[3];
    }
    Resources::setRoot(argv[1]);
    dawnstar_init_statics(platform::defaultContext());

    const int64_t seeds[] = {0, 1, 42, 8000, 16000, 296000, -1};
    for (int64_t seed : seeds) {
        GameRandom rnd(seed);
        std::vector<int32_t> vals;
        for (int n1 = 0; n1 < 8; ++n1) vals.push_back(rnd.nextInt());
        row("random\t" + std::to_string(seed) + "\t" + joinInts(vals));
    }

    const int32_t bounds[] = {2, 3, 10, 17, 100, 1000, 10000};
    for (int32_t bound : bounds) {
        GameRandom rnd(12345);
        std::vector<int32_t> vals;
        for (int n2 = 0; n2 < 8; ++n2) vals.push_back(GameUtil::randomInt(&rnd, bound));
        row("lingo\t" + i2s(bound) + "\t" + joinInts(vals));
    }

    struct LongCase { const char *hex; int32_t off; };
    const LongCase longCases[] = {
        {"0000000000000000", 0}, {"ffffffffffffffff", 0}, {"8000000000000000", 0},
        {"0123456789abcdef", 0}, {"55deadbeef01234567aa", 1}, {"00ff00ff00ff00ff", 0}};
    for (const LongCase &lc : longCases) {
        std::string hex = lc.hex;
        SharedArray<int8_t> bytes((int32_t)(hex.size() / 2));
        for (int32_t n3 = 0; n3 < bytes.length(); ++n3) {
            bytes[n3] = (int8_t)std::stoul(hex.substr((size_t)n3 * 2, 2), nullptr, 16);
        }
        row("f.readLong\t" + hex + "\t" + i2s(lc.off) + "\t" + longHex(GameUtil::readLongBE(bytes, lc.off)));
    }

    for (int32_t bit = 0; bit < 8; ++bit) {
        row("f.setBit\t" + i2s(bit) + "\t" + i2s(GameUtil::setBit(bit, 0)));
        row("f.clearBit\t" + i2s(bit) + "\t" + i2s(GameUtil::clearBit(bit, -1)));
    }

    const int32_t flagVals[] = {0, 1, 2, 4, 8, 16, 32, 64, -1};
    for (int32_t x : flagVals) {
        for (int32_t y : flagVals) {
            int8_t mask = (int8_t)x;
            int8_t val = (int8_t)y;
            row("f.flags\t" + i2s(x) + "\t" + i2s(y) + "\t" +
                i2s((int32_t)GameUtil::setFlag(mask, val)) + "\t" + i2s((int32_t)GameUtil::clearFlag(mask, val)) + "\t" +
                (GameUtil::hasFlag(mask, val) ? "true" : "false"));
        }
    }

    const int32_t coords[][2] = {{0, 0}, {1, 2}, {17, 5}, {34, 34}, {-1, -1}};
    for (const auto &c1 : coords) {
        row("f.coordKey\t" + i2s(c1[0]) + "\t" + i2s(c1[1]) + "\t" +
            GameUtil::coordKey(c1[0], c1[1]));
    }

    const char *repCases[][3] = {{"Your gold: <TAG>", "<TAG>", "250"},
                                 {"<TAG> and <TAG>", "<TAG>", "x"},
                                 {"nothing here", "<TAG>", "y"},
                                 {"<TAG>", "<TAG>", ""}};
    for (const auto &rc : repCases) {
        row(std::string("f.replace\t") + rc[0] + "\t" + rc[1] + "\t" + rc[2] + "\t" +
            GameUtil::replace(std::string(rc[0]), std::string(rc[1]), std::string(rc[2])));
    }

    rowStrings("a.l", itemEffectText);
    rowInts("c.d", DungeonGen::kChestTier, 36);
    rowStrings("i.n", Dungeon::dungeonNames_);
    for (int32_t n4 = 0; n4 < 37; ++n4) rowInts("i.m[" + i2s(n4) + "]", Dungeon::kSpawnTable[n4], 4);
    rowInts("i.i", Dungeon::kDungeonLevel, 36);
    rowStrings("k.r", NpcSystem::npcNames_);
    rowIntsArr("k.c", NpcSystem::npcKind_);
    rowIntsArr("k.f", NpcSystem::npcGridX_);
    rowIntsArr("k.e", NpcSystem::npcGridY_);
    {
        std::vector<int32_t> v;
        for (int32_t n5 = 0; n5 < NpcSystem::dialogueCounts_.length(); ++n5) v.push_back(NpcSystem::dialogueCounts_[n5]);
        row("k.l\t" + i2s(NpcSystem::dialogueCounts_.length()) + "\t" + joinInts(v));
    }
    for (int32_t n6 = 0; n6 < NpcSystem::stockLists_.length(); ++n6) rowIntsArr("k.n[" + i2s(n6) + "]", NpcSystem::stockLists_[n6]);
    for (int32_t n7 = 0; n7 < 4; ++n7) rowInts("k.m[" + i2s(n7) + "]", NpcSystem::kTraitorRumors[n7], 6);
    rowInts("k.a", NpcSystem::kClueTrue, 24);
    rowInts("k.i", NpcSystem::kClueFalse, 24);

    {
        std::vector<int32_t> v;
        for (int32_t x = 0; x < 5; ++x)
            for (int32_t y = 0; y < 6; ++y)
                for (int32_t z = 0; z < 4; ++z) v.push_back(GameCanvas::kWallLookup[x][y][z]);
        row("e.k	" + i2s((int32_t)v.size()) + "	" + joinInts(v));
    }
    {
        std::vector<int32_t> v;
        for (int32_t x = 0; x < 41; ++x)
            for (int32_t y = 0; y < 2; ++y) v.push_back((int32_t)profile().spriteDrawMode[x][y]);
        row("e.a	" + i2s((int32_t)v.size()) + "	" + joinInts(v));
    }
    {
        std::vector<int32_t> v;
        for (int32_t x = 0; x < 41; ++x)
            for (int32_t y = 0; y < 4; ++y) v.push_back(profile().spriteParts[x][y] ? 1 : 0);
        row("e.G	" + i2s((int32_t)v.size()) + "	" + joinInts(v));
    }
    rowInts("e.L", GameCanvas::kFacingChars, 5);
    rowInts("e.e", GameCanvas::kIconLabels, 6);
    {
        std::vector<int32_t> v;
        for (int32_t x = 0; x < 7; ++x)
            for (int32_t y = 0; y < 2; ++y) v.push_back(Player::startingKit_[x][y]);
        row("j.q	" + i2s((int32_t)v.size()) + "	" + joinInts(v));
    }

    int status = emit(mode, path);
    std::fflush(stdout);
    std::_Exit(status);
}
