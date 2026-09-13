#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/host/game_host.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/headless.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/profile.hpp"
#include "src/stormhold/variant.hpp"

namespace {

int failures = 0;

void check(bool value, const char *message) {
    if (!value) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

const int32_t KEY_DOWN = -2;
const int32_t KEY_SOFT_RIGHT = -7;

int64_t g_now = 1000000000000LL;
int64_t botClock() { return g_now; }
void botSleep(int64_t ms) {
    if (ms > 0) g_now += ms;
}

int32_t g_pendingJob = 0;
bool g_hasPendingJob = false;
void queueHelperJob(Game *, int32_t job) {
    g_pendingJob = job;
    g_hasPendingJob = true;
}
void drainPendingWork(Game *game) {
    while (g_hasPendingJob) {
        g_hasPendingJob = false;
        game->runHelperJob(g_pendingJob);
    }
}
void primeCanvasLoop(GameCanvas *canvas) { canvas->running_ = true; }
void noSplashLoop(UIWidget *) {}

void press(Game *game, int32_t code) {
    if (Canvas *canvas = game->display_ != nullptr
                             ? dynamic_cast<Canvas *>(game->display_->getCurrent())
                             : nullptr) {
        canvas->keyPressed(code);
        canvas->keyReleased(code);
    }
}

bool createCharacter(Game *game) {
    game->startApplication();
    drainPendingWork(game);
    if (game->splashUI_ == nullptr) return false;
    headless::finishSplash(game->splashUI_);
    const int32_t flow[] = {KEY_SOFT_RIGHT, KEY_SOFT_RIGHT, KEY_DOWN, KEY_SOFT_RIGHT,
                            KEY_SOFT_RIGHT};
    for (int32_t code : flow) {
        press(game, code);
        drainPendingWork(game);
    }
    if (!host::completeNameForm(game->display_, "Bot")) return false;
    drainPendingWork(game);
    for (int n = 0; n < 2; ++n) {
        press(game, KEY_SOFT_RIGHT);
        drainPendingWork(game);
    }
    return game->gameCanvas_ != nullptr && game->character_ != nullptr;
}

struct Worth {
    int32_t chests = 0;
    int32_t goods = 0;
    int32_t points = 0;
};

Worth worthOf(Game &game, int32_t id) {
    Worth out;
    const std::size_t index = (std::size_t)(id - 1);
    if (game.worldState().chests.hasTable(index)) {
        for (const worldstate::ChestRecord &c : game.worldState().chests.at(index)) {
            ++out.chests;
            const int32_t item = (int32_t)c[4] - 1;
            if (item < 0 || item >= Items::count_) continue;
            if (Items::at(item).category != 11) continue;
            ++out.goods;
            out.points += Items::at(item).tier;
        }
    }
    for (const worldstate::DroppedItemRecord &d :
         game.worldState().droppedItems.at(index)) {
        const int32_t item = (int32_t)d[2] - 1;
        if (item < 0 || item >= Items::count_) continue;
        if (Items::at(item).category != 11) continue;
        ++out.goods;
        out.points += Items::at(item).tier;
    }
    return out;
}

}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-budget"
                                         : std::string("saves/rms-budget"));

    render::Surface screen(176, 208);
    render::Context renderer(&screen);
    platform::PlatformContext context;
    context.installFileSystem(platform::defaultContext()->fileSystem());
    context.installSaveStore(platform::defaultContext()->saveStore());
    context.installRenderServices(&renderer);
    context.installClock(botClock);
    context.installSleep(botSleep);

    stormhold::stormhold_init_statics(&context);
    Game game(stormhold::profile(), &context);
    game.setExecutionHooks(queueHelperJob, primeCanvasLoop, noSplashLoop);
    check(createCharacter(&game), "a character was created");
    if (failures != 0) return 1;

    const int32_t kThreshold[9] = {0, 9, 13, 17, 23, 28, 34, 40, 48};

    int32_t running = 0;
    int32_t cumulative[9] = {0};
    for (int32_t level = 0; level <= 8; ++level) {
        game.openAndRepopulateDungeons(level);
        int32_t groupPoints = 0, groupGoods = 0, groupChests = 0;
        for (int32_t id = 1; id <= (int32_t)worldstate::DungeonRegistry::kDungeonCount;
             ++id) {
            if (!game.dungeonAt(id)->populated_) continue;
            const Worth w = worthOf(game, id);
            groupChests += w.chests;
            groupGoods += w.goods;
            groupPoints += w.points;
        }
        running = groupPoints;
        cumulative[level] = running;
        std::printf("level %d (needs %2d): world holds %3d chests, %3d trade goods, "
                    "%3d gift points\n",
                    level, kThreshold[level], groupChests, groupGoods, running);
    }

    std::printf("\n");
    bool stuck = false;
    for (int32_t level = 0; level < 8; ++level) {
        const int32_t have = cumulative[level];
        const int32_t need = kThreshold[level + 1];
        const bool ok = have >= need;
        std::printf("from level %d: %3d points available, %2d needed for level %d -- %s\n",
                    level, have, need, level + 1, ok ? "reachable" : "STUCK");
        if (!ok) stuck = true;
    }

    std::printf("\ntotal gift points in the whole world: %d (48 opens the last group)\n",
                cumulative[8]);

    check(!stuck, "every advancement level is reachable from the loot already open");
    check(cumulative[8] >= 48, "the world holds enough gift points to open the last group");

    if (failures == 0) std::printf("gift budget: OK\n");
    return failures == 0 ? 0 : 1;
}
