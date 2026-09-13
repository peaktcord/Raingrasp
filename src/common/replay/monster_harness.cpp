#include "src/common/replay/monster_harness.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/runtime.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/save_records.hpp"
#include "src/common/replay/headless.hpp"

namespace monster_harness {
namespace {

void row(std::string &out, const std::string &line) {
    out += line;
    out += "\n";
}

std::string i2s(int32_t value) { return std::to_string(value); }

void dumpMovement(std::string &out, DungeonCore *dungeon, int32_t type) {
    for (int32_t x = 8; x <= 12; ++x) {
        for (int32_t y = 8; y <= 12; ++y) {
            for (int32_t direction = 1; direction <= 4; ++direction) {
                Monster *monster = new Monster(1, type, 1);
                monster->dungeon_ = dungeon;
                monster->gridX_ = (int8_t)x;
                monster->gridY_ = (int8_t)y;
                int8_t beforeX = monster->gridX_;
                int8_t beforeY = monster->gridY_;
                bool moved = monster->stepDir(direction);
                row(out, std::string("move\ttype") + i2s(type) + "\t" + i2s(x) +
                             "," + i2s(y) + "\tdir" + i2s(direction) + "\t" +
                             (moved ? "1" : "0") + "\t" + i2s((int32_t)beforeX) +
                             "," + i2s((int32_t)beforeY) + "->" +
                             i2s((int32_t)monster->gridX_) + "," +
                             i2s((int32_t)monster->gridY_));
            }
        }
    }
}

void dumpStats(std::string &out, Game *game, int32_t type) {
    Monster *monster = new Monster(1, type, 1);
    monster->gridX_ = 10;
    monster->gridY_ = 10;
    row(out, std::string("stats\ttype") + i2s(type) + "\t" +
                 i2s((int32_t)monster->type_) + "\t" + i2s((int32_t)monster->gridX_) +
                 "\t" + i2s((int32_t)monster->gridY_) + "\t" +
                 i2s((int32_t)game->worldState_.nextMonsterUid()) + "\t" +
                 (monster->isUndead() ? "1" : "0"));
    row(out, std::string("name\ttype") + i2s(type) + "\t" +
                 monster->name());
}

void dumpRecordRoundTrip(std::string &out, int32_t type) {
    Monster *before = new Monster(1, type, 1);
    before->gridX_ = 17;
    before->gridY_ = 23;
    SharedArray<int8_t> record = before->toRecord();

    std::string bytes;
    for (int32_t index = 0; index < record.length(); ++index) {
        if (index) bytes += ",";
        bytes += std::to_string((int32_t)record[index]);
    }
    row(out, std::string("record\ttype") + i2s(type) + "\t" +
                 i2s(record.length()) + "\t" + bytes);

    Monster decoded;
    Monster *after = Monster::fromRecord(&decoded, record);
    row(out, std::string("record_load\ttype") + i2s(type) + "\t" +
                 i2s((int32_t)after->type_) + "\t" + i2s((int32_t)after->gridX_) +
                 "\t" + i2s((int32_t)after->gridY_) + "\t" +
                 ((before->type_ == after->type_ &&
                   before->gridX_ == after->gridX_ &&
                   before->gridY_ == after->gridY_)
                      ? "same"
                      : "LOST"));
}

int emit(const std::string &out, const char *mode, const char *path) {
    if (std::string(mode) == "--write") {
        std::FILE *file = std::fopen(path, "wb");
        if (file == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", path);
            return 2;
        }
        std::fwrite(out.data(), 1, out.size(), file);
        std::fclose(file);
        std::printf("wrote %s\n", path);
        return 0;
    }

    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot read baseline %s\n", path);
        return 2;
    }
    std::string expected;
    char buffer[4096];
    size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        expected.append(buffer, count);
    }
    std::fclose(file);

    std::string got = out;
    got.erase(std::remove(got.begin(), got.end(), '\r'), got.end());
    expected.erase(std::remove(expected.begin(), expected.end(), '\r'), expected.end());
    if (got == expected) {
        size_t rows = 0;
        for (char value : got) {
            if (value == '\n') ++rows;
        }
        std::printf("monsters match the baseline (%zu rows)\n", rows);
        return 0;
    }

    std::vector<std::string> actualLines;
    std::vector<std::string> expectedLines;
    for (std::string *source : {&got, &expected}) {
        std::vector<std::string> &lines =
            source == &got ? actualLines : expectedLines;
        std::string line;
        for (char value : *source) {
            if (value == '\n') {
                lines.push_back(line);
                line.clear();
            } else {
                line += value;
            }
        }
        if (!line.empty()) lines.push_back(line);
    }

    std::printf("FAIL: monster behaviour changed\n");
    std::printf("  NOTE: if the `move` rows reversed for directions 1 and 4, the\n");
    std::printf("        switch fallthrough in d::a(int) was broken. See the header.\n");
    int shown = 0;
    for (size_t index = 0;
         index < actualLines.size() || index < expectedLines.size(); ++index) {
        const std::string &actual =
            index < actualLines.size() ? actualLines[index] : std::string();
        const std::string &expectedLine =
            index < expectedLines.size() ? expectedLines[index] : std::string();
        if (actual != expectedLine) {
            if (shown == 0) {
                std::printf("  first difference at line %zu\n", index + 1);
            }
            if (shown < 5) {
                std::printf("    expected: %s\n", expectedLine.c_str());
                std::printf("    actual:   %s\n", actual.c_str());
            }
            ++shown;
        }
    }
    std::printf("  %d line(s) differ; %zu expected, %zu produced\n", shown,
                expectedLines.size(), actualLines.size());
    return 1;
}

}

int run(int argc, char **argv, const game::Profile &profile) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "usage: %s <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }

    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot(std::string("saves/rms-monsterdump-") + profile.name);
    profile.initStatics(platform::defaultContext());

    Game *game = new Game(profile, platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();
    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::fprintf(stderr, "FAIL: appload did not complete\n");
        return 1;
    }
    game->loadGameUI_ = game->makeOwnedUIWidget(9, uistate::SCREEN_PROGRESS_LOAD_GAME);
    for (int32_t level = 0; level <= 8; ++level) {
        game->openAndRepopulateDungeons(level);
    }

    std::string out;
    row(out, "# monsters: movement (the d::a(int) fallthrough), stats, byte records.");

    DungeonCore *dungeon = game->dungeons_[0];
    const int32_t types[] = {1, 5, 11, 20, 33};
    for (int32_t type : types) {
        dumpStats(out, game, type);
        dumpRecordRoundTrip(out, type);
    }
    for (int32_t type : types) {
        dumpMovement(out, dungeon, type);
    }

    int status = emit(out, argv[2], argv[3]);
    std::fflush(stdout);
    std::_Exit(status);
}

}
