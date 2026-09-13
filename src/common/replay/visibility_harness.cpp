#include "src/common/replay/visibility_harness.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/player.hpp"
#include "src/common/runtime.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/save_records.hpp"
#include "src/common/replay/headless.hpp"

namespace visibility_harness {
namespace {

void row(std::string &out, const std::string &line) {
    out += line;
    out += "\n";
}

std::string i2s(int32_t v) { return std::to_string(v); }

constexpr int32_t kSlotCount = 13;

std::string slotText(const visibility::Slot &slot) {
    if (slot.is(visibility::SlotState::Empty)) return ".";
    if (slot.is(visibility::SlotState::Wall)) return "#";
    if (slot.is(visibility::SlotState::Occluded)) return "X";
    if (const SharedArray<int8_t> *bytes = slot.record()) {
        std::string text = "b" + i2s(bytes->length()) + ":";
        for (int32_t n = 0; n < bytes->length() && n < 8; ++n) {
            if (n) text += ",";
            text += i2s((int32_t)(*bytes)[n]);
        }
        return text;
    }
    if (const std::string *npc = slot.npc()) return "s:" + *npc;
    return "?";
}

std::string slotsText(Player *player) {
    std::string text;
    for (int32_t n = 0; n < kSlotCount; ++n) {
        if (n) text += " ";
        text += slotText(player->visibleSlots_[(size_t)n]);
    }
    return text;
}

void setSurroundings(Player *player, const char *walls) {
    player->surroundings_ = makeSharedArray2D<int8_t>(7, 7);
    for (int32_t y = 0; y < 7; ++y) {
        for (int32_t x = 0; x < 7; ++x) {
            char cell = walls[y * 7 + x];
            player->surroundings_[x][y] = (int8_t)(cell == '#' ? 1 : 0);
        }
    }
}

SharedArray<int8_t> tileObject(int32_t x, int32_t y, int32_t tag) {
    SharedArray<int8_t> bytes(7);
    bytes[0] = (int8_t)x;
    bytes[1] = (int8_t)y;
    bytes[2] = (int8_t)tag;
    return bytes;
}

void dumpReset(std::string &out, const char *fixture, Player *player) {
    for (int32_t facing = 1; facing <= 4; ++facing) {
        player->facing_ = (int8_t)facing;
        player->resetVisible();
        row(out, std::string("reset\t") + fixture + "\tf" + i2s(facing) + "\t" +
                     slotsText(player));
    }
}

void dumpPlace(std::string &out, const char *fixture, Player *player, int32_t kind,
               const visibility::Slot &object) {
    for (int32_t facing = 1; facing <= 4; ++facing) {
        player->facing_ = (int8_t)facing;
        player->resetVisible();
        bool placed = player->placeVisible(kind, object);
        int32_t where = -1;
        for (int32_t n = 0; n < kSlotCount; ++n) {
            if (player->visibleSlots_[(size_t)n] == object) {
                where = n;
                break;
            }
        }
        row(out, std::string("place\t") + fixture + "\tf" + i2s(facing) + "\t" +
                     i2s(kind) + "\t" + (placed ? "1" : "0") + "\tslot" + i2s(where) +
                     "\t" + slotsText(player));
    }
}

void dumpContest(std::string &out, const char *fixture, Player *player, int32_t kind,
                 const visibility::Slot &first, const visibility::Slot &second) {
    for (int32_t facing = 1; facing <= 4; ++facing) {
        player->facing_ = (int8_t)facing;
        player->resetVisible();
        bool firstPlaced = player->placeVisible(kind, first);
        bool secondPlaced = player->placeVisible(kind, second);
        row(out, std::string("contest\t") + fixture + "\tf" + i2s(facing) + "\t" +
                     (firstPlaced ? "1" : "0") + (secondPlaced ? "1" : "0") + "\t" +
                     slotsText(player));
    }
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
        for (char c : got) {
            if (c == '\n') ++rows;
        }
        std::printf("visibility matches the baseline (%zu rows)\n", rows);
        return 0;
    }

    std::vector<std::string> actualLines;
    std::vector<std::string> expectedLines;
    for (std::string *source : {&got, &expected}) {
        std::vector<std::string> &lines = source == &got ? actualLines : expectedLines;
        std::string line;
        for (char c : *source) {
            if (c == '\n') {
                lines.push_back(line);
                line.clear();
            } else {
                line += c;
            }
        }
        if (!line.empty()) lines.push_back(line);
    }

    std::printf("FAIL: the visibility slots changed\n");
    int shown = 0;
    for (size_t i = 0; i < actualLines.size() || i < expectedLines.size(); ++i) {
        const std::string &actual =
            i < actualLines.size() ? actualLines[i] : std::string();
        const std::string &expectedLine =
            i < expectedLines.size() ? expectedLines[i] : std::string();
        if (actual != expectedLine) {
            if (shown == 0) std::printf("  first difference at line %zu\n", i + 1);
            if (shown < 6) {
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
        std::fprintf(stderr, "usage: %s <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }

    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot(std::string("saves/rms-visdump-") + profile.name);
    profile.initStatics(platform::defaultContext());
    Game *game = new Game(profile, platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();

    Player *player = new Player(game);
    Player *otherPlayer = new Player(game);
    player->gameWon_ = true;
    if (otherPlayer->gameWon_ ||
        &player->visibleSlots_ == &otherPlayer->visibleSlots_) {
        std::fprintf(stderr, "FAIL: player visibility/win state is shared\n");
        return 1;
    }
    player->gameWon_ = false;
    player->initFromClass(0);
    player->dungeonId_ = 1;
    player->gridX_ = 10;
    player->gridY_ = 10;

    std::string out;
    row(out,
        "# visibility slots. '.' empty, '#' wall, 'X' occluded; see "
        "visibility_dump.cpp.");
    row(out,
        "# columns are the 13 slots in order; every fixture runs at all four "
        "facings.");

    static const char *kOpen =
        "......."
        "......."
        "......."
        "......."
        "......."
        "......."
        ".......";
    static const char *kNearCentre =
        "......."
        "..#...."
        "......."
        "......."
        "......."
        "......."
        ".......";
    static const char *kNearLeft =
        "......."
        ".#....."
        "......."
        "......."
        "......."
        "......."
        ".......";
    static const char *kNearRight =
        "......."
        "...#..."
        "......."
        "......."
        "......."
        "......."
        ".......";
    static const char *kNearBoth =
        "......."
        ".#.#..."
        "......."
        "......."
        "......."
        "......."
        ".......";
    static const char *kMidCentre =
        "......."
        "......."
        "...#..."
        "......."
        "......."
        "......."
        ".......";
    static const char *kMidFlanks =
        "......."
        "......."
        "..#.#.."
        "......."
        "......."
        "......."
        ".......";
    static const char *kFarRow =
        "......."
        "......."
        "......."
        ".#####."
        "......."
        "......."
        ".......";
    static const char *kDense =
        "......."
        ".#.#..."
        "..#.#.."
        ".#.#.#."
        "......."
        "......."
        ".......";
    static const char *kDeep =
        "......."
        "......."
        "......."
        "......."
        ".#####."
        ".#####."
        ".#####.";

    struct Pattern {
        const char *name;
        const char *grid;
    };
    const Pattern kPatterns[] = {
        {"open", kOpen},           {"nearcentre", kNearCentre},
        {"nearleft", kNearLeft},   {"nearright", kNearRight},
        {"nearboth", kNearBoth},   {"midcentre", kMidCentre},
        {"midflanks", kMidFlanks}, {"farrow", kFarRow},
        {"dense", kDense},         {"deep", kDeep},
    };

    for (const Pattern &pattern : kPatterns) {
        setSurroundings(player, pattern.grid);
        dumpReset(out, pattern.name, player);
    }

    for (const Pattern &pattern : kPatterns) {
        setSurroundings(player, pattern.grid);
        for (int32_t dy = -3; dy <= 3; ++dy) {
            for (int32_t dx = -3; dx <= 3; ++dx) {
                std::string fixture = std::string(pattern.name) + "_dx" + i2s(dx) +
                                      "_dy" + i2s(dy);
                SharedArray<int8_t> object =
                    tileObject(player->gridX_ + dx, player->gridY_ + dy, 42);
                dumpPlace(out, fixture.c_str(), player, 4, visibility::Slot(object));
            }
        }
    }

    setSurroundings(player, kOpen);
    for (int32_t dy = -2; dy <= 2; ++dy) {
        for (int32_t dx = -2; dx <= 2; ++dx) {
            std::string fixture =
                std::string("contest_dx") + i2s(dx) + "_dy" + i2s(dy);
            SharedArray<int8_t> first =
                tileObject(player->gridX_ + dx, player->gridY_ + dy, 1);
            SharedArray<int8_t> second =
                tileObject(player->gridX_ + dx, player->gridY_ + dy, 2);
            dumpContest(out, fixture.c_str(), player, 4, visibility::Slot(first), visibility::Slot(second));
        }
    }

    setSurroundings(player, kOpen);
    {
        SharedArray<int8_t> monster(28);
        monster[0] = 99;
        monster[1] = 99;
        monster[4] = (int8_t)player->gridX_;
        monster[5] = (int8_t)(player->gridY_ - 1);
        dumpPlace(out, "monster_kind1", player, 1, visibility::Slot(monster));

        SharedArray<int8_t> chest =
            tileObject(player->gridX_, player->gridY_ - 1, 7);
        dumpPlace(out, "chest_kind4", player, 4, visibility::Slot(chest));

        SharedArray<int8_t> dropped =
            tileObject(player->gridX_, player->gridY_ - 1, 8);
        dumpPlace(out, "dropped_kind2", player, 2, visibility::Slot(dropped));
    }

    int status = emit(out, argv[2], argv[3]);
    std::fflush(stdout);
    return status;
}

}
