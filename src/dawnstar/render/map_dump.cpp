#include <cstdio>
#include <string>
#include <vector>

#include "src/common/platform/desktop.hpp"
#include "src/common/render/map_atlas.hpp"
#include "src/common/replay/headless.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/dawnstar/profile.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/dawnstar/variant.hpp"
#include "src/common/game/monster.hpp"

using namespace dawnstar;

namespace {

std::string sanitize(const std::string &name) {
    std::string out;
    for (char ch : name) {
        out += (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
                   ? ch
                   : '_';
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? std::string("unnamed") : out;
}

mapatlas::Dungeon collect(Game &game, int32_t id) {
    Dungeon *source = game.dungeons_.as<Dungeon>((std::size_t)(id - 1));
    mapatlas::Dungeon dungeon;
    dungeon.id = id;
    dungeon.width = source->width_;
    dungeon.height = source->height_;
    dungeon.name = source->name();
    dungeon.cells.resize((size_t)(dungeon.width * dungeon.height));
    for (int32_t x = 0; x < dungeon.width; ++x) {
        for (int32_t y = 0; y < dungeon.height; ++y) {
            dungeon.cells[(size_t)(x * dungeon.height + y)] = source->tiles_[x][y];
        }
    }
    for (int32_t edge = 0; edge < 4; ++edge) {
        dungeon.links[edge] = source->geometry_[edge];
    }

    const std::size_t monsterDungeonIndex = (std::size_t)(id - 1);
    if (game.worldState_.monsters.hasTable(monsterDungeonIndex)) {
        for (const SharedArray<int8_t> &record :
             game.worldState_.monsters.at(monsterDungeonIndex)) {
            mapatlas::Marker marker;
            marker.x = record[4];
            marker.y = record[5];
            marker.kind = mapatlas::MONSTER;
            dungeon.markers.push_back(marker);
        }
    }
    const std::size_t dungeonIndex = (std::size_t)(id - 1);
    if (game.worldState_.chests.hasTable(dungeonIndex)) {
        for (const SharedArray<int8_t> &record : game.worldState_.chests.at(dungeonIndex)) {
            mapatlas::Marker marker;
            marker.x = record[0];
            marker.y = record[1];
            marker.kind = mapatlas::CHEST;
            dungeon.markers.push_back(marker);
        }
    }
    for (const SharedArray<int8_t> &record :
         game.worldState_.droppedItems.at((std::size_t)(id - 1))) {
        mapatlas::Marker marker;
        marker.x = record[0];
        marker.y = record[1];
        marker.kind = mapatlas::ITEM;
        dungeon.markers.push_back(marker);
    }
    return dungeon;
}

int32_t countKind(const mapatlas::Dungeon &dungeon, mapatlas::MarkerKind kind) {
    int32_t n1 = 0;
    for (size_t m = 0; m < dungeon.markers.size(); ++m) {
        if (dungeon.markers[m].kind == kind) ++n1;
    }
    return n1;
}

int32_t countNpcs(const mapatlas::Dungeon &dungeon) {
    int32_t n1 = 0;
    for (int32_t x = 0; x < dungeon.width; ++x) {
        for (int32_t y = 0; y < dungeon.height; ++y) {
            if ((dungeon.at(x, y) & 0x20) != 0) ++n1;
        }
    }
    return n1;
}

}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <resource-dir> <out-dir> [save-dir]\n", argv[0]);
        return 2;
    }
    std::string outDir = argv[2];
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot(argc >= 4 ? argv[3] : "saves/rms");

    dawnstar_init_statics(platform::defaultContext());
    Game *game = new Game(profile(), platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();

    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::printf("FAIL: appload did not complete\n");
        return 1;
    }

    std::printf("opening dungeons\n");
    for (int32_t n1 = 0; n1 < 37; ++n1) {
        Dungeon *dungeon = game->dungeons_.as<Dungeon>((std::size_t)n1);
        dungeon->populated_ = true;
        dungeon->rebuildOccupancy();
    }

    std::printf("maps\n");
    std::vector<mapatlas::Dungeon> all;
    int32_t monsters = 0;
    int32_t chests = 0;
    int32_t items = 0;
    int32_t failures = 0;
    for (int32_t id = 1; id <= 37; ++id) {
        mapatlas::Dungeon dungeon = collect(*game, id);
        int32_t m = countKind(dungeon, mapatlas::MONSTER);
        int32_t c1 = countKind(dungeon, mapatlas::CHEST);
        int32_t t = countKind(dungeon, mapatlas::ITEM);
        monsters += m;
        chests += c1;
        items += t;

        char index[8];
        std::snprintf(index, sizeof(index), "%02d", (int)id);
        std::string path = outDir + "/" + index + "_" + sanitize(dungeon.name) + ".png";
        bool ok = mapatlas::writeMap(dungeon, path, 8);
        if (!ok) ++failures;
        std::printf("  %2d. %-24s %s  monsters %3d  chests %3d  items %3d  npcs %d\n", (int)id,
                    dungeon.name.c_str(), ok ? "wrote" : "FAILED", (int)m, (int)c1, (int)t,
                    (int)countNpcs(dungeon));
        std::fflush(stdout);
        all.push_back(dungeon);
    }

    std::vector<std::string> conflicts;
    std::string atlasPath = outDir + "/00_all_dungeons.png";
    bool ok = mapatlas::writeAtlas(all, atlasPath, 4, &conflicts);
    if (!ok) ++failures;
    std::printf("\n  00_all_dungeons          %s\n", ok ? "wrote" : "FAILED");
    if (!conflicts.empty()) {
        std::printf("  the world does not tile flat -- %d edge(s) disagree:\n",
                    (int)conflicts.size());
        for (size_t n2 = 0; n2 < conflicts.size(); ++n2) {
            std::printf("    %s\n", conflicts[n2].c_str());
        }
    }
    std::printf("\n37 maps, %d monsters, %d chests, %d dropped items placed\n", (int)monsters,
                (int)chests, (int)items);
    if (failures != 0) {
        std::printf("%d map(s) failed to write\n", (int)failures);
        std::fflush(stdout);
        std::_Exit(1);
    }
    std::fflush(stdout);
    std::_Exit(0);
}
