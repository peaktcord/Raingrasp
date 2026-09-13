#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "src/common/platform/desktop.hpp"
#include "src/common/replay/headless.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/stormhold/profile.hpp"
#include "src/stormhold/variant.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/ui_widget.hpp"

using namespace stormhold;

namespace {

std::string g_out;

void row(const std::string &line) {
    g_out += line;
    g_out += "\n";
}

std::string i2s(int32_t v) { return std::to_string(v); }

std::string dungeonName(Dungeon *dungeon) {
    SharedArray<std::string> parts = dungeon->name();
    std::string out;
    for (int32_t n1 = 0; n1 < parts.length(); ++n1) {
        std::string word = parts[n1];
        if (word.empty()) continue;
        if (!out.empty()) out += " ";
        out += word;
    }
    return out;
}

unsigned long layoutDigest(Dungeon *dungeon) {
    unsigned long hash = 1469598103u;
    for (int32_t x = 0; x < dungeon->width_; ++x) {
        for (int32_t y = 0; y < dungeon->height_; ++y) {
            hash = (hash ^ (unsigned char)dungeon->tiles_[x][y]) * 16777619u;
        }
    }
    return hash & 0xFFFFFFFFul;
}

unsigned long flagDigest(Dungeon *dungeon) {
    unsigned long hash = 2166136261u;
    for (int32_t x = 0; x < dungeon->width_; ++x) {
        for (int32_t y = 0; y < dungeon->height_; ++y) {
            unsigned char cell = (unsigned char)dungeon->tiles_[x][y];
            if (cell != 0) {
                hash = (hash ^ (unsigned char)x) * 16777619u;
                hash = (hash ^ (unsigned char)y) * 16777619u;
                hash = (hash ^ cell) * 16777619u;
            }
        }
    }
    return hash & 0xFFFFFFFFul;
}

int emit(const char *mode, const char *path) {
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
        std::printf("dungeon layouts match the baseline (%zu bytes)\n", got.size());
        return 0;
    }
    std::vector<std::string> a, b;
    for (std::string *src : {&got, &expected}) {
        std::vector<std::string> &out = (src == &got) ? a : b;
        std::string line;
        for (char c1 : *src) {
            if (c1 == '\n') { out.push_back(line); line.clear(); } else { line += c1; }
        }
        if (!line.empty()) out.push_back(line);
    }
    std::printf("FAIL: dungeon generation changed\n");
    int shown = 0;
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const std::string &ga = i < a.size() ? a[i] : std::string();
        const std::string &gb = i < b.size() ? b[i] : std::string();
        if (ga != gb) {
            if (shown == 0) std::printf("  first difference at line %zu\n", i + 1);
            if (shown < 5) {
                std::printf("    expected: %s\n", gb.c_str());
                std::printf("    actual:   %s\n", ga.c_str());
            }
            ++shown;
        }
    }
    std::printf("  %d line(s) differ; %zu expected, %zu produced\n", shown, b.size(), a.size());
    return 1;
}

}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/rms-dungeondump-sh");
    stormhold_init_statics(platform::defaultContext());
    Game *game = new Game(profile(), platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();
    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::fprintf(stderr, "FAIL: appload did not complete\n");
        return 1;
    }

    for (int32_t level = 0; level <= 8; ++level) {
        game->openAndRepopulateDungeons(level);
    }

    row("# columns: dungeon<TAB>id<TAB>name<TAB>w<TAB>h<TAB>layout<TAB>flags<TAB>monsters<TAB>chests");
    int32_t count = (int32_t)game->dungeons_.size();
    row(std::string("count\t") + i2s(count));
    for (int32_t n1 = 0; n1 < count; ++n1) {
        Dungeon *dungeon = game->dungeons_.as<Dungeon>((std::size_t)n1);
        if (dungeon == nullptr) {
            row(std::string("dungeon\t") + i2s(n1) + "\t(null)");
            continue;
        }
        char digest[32];
        std::snprintf(digest, sizeof(digest), "%08lx", layoutDigest(dungeon));
        char flags[32];
        std::snprintf(flags, sizeof(flags), "%08lx", flagDigest(dungeon));
        int32_t monsters = -1;
        int32_t chests = -1;
        if (game->worldState_.monsters.hasTable((std::size_t)n1)) {
            monsters = (int32_t)game->worldState_.monsters.at((std::size_t)n1).size();
        }
        if (game->worldState_.chests.hasTable((std::size_t)n1)) {
            chests = (int32_t)game->worldState_.chests.at((std::size_t)n1).size();
        }
        row(std::string("dungeon\t") + i2s(n1) + "\t" + dungeonName(dungeon) + "\t" +
            i2s(dungeon->width_) + "\t" + i2s(dungeon->height_) + "\t" + digest + "\t" + flags + "\t" +
            i2s(monsters) + "\t" + i2s(chests));
    }

    int status = emit(argv[2], argv[3]);
    std::fflush(stdout);
    std::_Exit(status);
}
