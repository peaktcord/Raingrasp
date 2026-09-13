#include <windows.h>
#include <psapi.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/common/bot/bot.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/host/game_host.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/alloc_trace.hpp"
#include "src/common/platform/crash_trace.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/headless.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/profile.hpp"
#include "src/stormhold/variant.hpp"

namespace {

using StormholdBot = bot::Bot<stormhold::Dungeon>;

const int32_t KEY_DOWN = -2;
const int32_t KEY_SOFT_RIGHT = -7;
const int64_t kTickMs = 250;

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

std::size_t workingSetMB() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return 0;
    return pmc.WorkingSetSize / (1024 * 1024);
}

void report(Game &game, const char *when) {
    std::size_t monsters = 0, chests = 0, dropped = 0;
    for (std::size_t i = 0; i < worldstate::DungeonRegistry::kDungeonCount; ++i) {
        if (game.worldState().monsters.hasTable(i)) {
            monsters += game.worldState().monsters.at(i).size();
        }
        if (game.worldState().chests.hasTable(i)) {
            chests += game.worldState().chests.at(i).size();
        }
        dropped += game.worldState().droppedItems.at(i).size();
    }
    std::printf("%-10s rss=%4zuMB monsters=%6zu chests=%5zu dropped=%6zu widgets=%4zu\n",
                when, workingSetMB(), monsters, chests, dropped,
                game.ownedUiWidgets_.size());
    std::fflush(stdout);
}

}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    crash_trace::install();

    if (HANDLE job = CreateJobObjectW(nullptr, nullptr)) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        limits.ProcessMemoryLimit = (SIZE_T)1024 * 1024 * 1024;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                                sizeof(limits));
        AssignProcessToJobObject(job, GetCurrentProcess());
    }
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-growth"
                                         : std::string("saves/rms-growth"));

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
    if (!createCharacter(&game)) {
        std::fprintf(stderr, "could not create a character\n");
        return 1;
    }

    StormholdBot bot(&game, &g_now, kTickMs);
    Player *player = bot.player();
    (void)player;
    report(game, "start");

    static Game *g_game = &game;
    static int64_t g_ticks = 0;
    bot.setFrameHook([](Game &g) {
        (void)g;
        if (++g_ticks % 2000 != 0) return;
        char label[32];
        std::snprintf(label, sizeof(label), "tick %lld", (long long)g_ticks);
        report(*g_game, label);
        if (workingSetMB() > 400) {
            alloc_trace::report("runaway");
            std::printf("stopping after %lld ticks\n", (long long)g_ticks);
            std::fflush(stdout);
            std::exit(3);
        }
    });
    bot.sweepAllDungeons();
    report(game, "end");
    return 0;
}
