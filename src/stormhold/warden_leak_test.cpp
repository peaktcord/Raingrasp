#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/host/game_host.hpp"
#include "src/common/runtime.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/crash_trace.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/headless.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/extension.hpp"
#include "src/stormhold/npc_script.hpp"
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
const int64_t kTickMs = 250;

const int32_t kTicks = 400;

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

}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    crash_trace::install();
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-warden"
                                         : std::string("saves/rms-warden"));

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

    Player *player = game.character_;

    // Exercise the complete gameplay-tick transition into Favela Dralor's
    // greeting.  A failure here used to look like a native crash because the
    // SDL host exited as soon as GameCanvas::tick reported an exception.
    game.gameCanvas_->npcAhead_ = 2;
    game.gameCanvas_->wantTalk_ = true;
    g_now += kTickMs;
    check(game.gameCanvas_->tick() != GameCanvas::TickStatus::Failed,
          "talking to Favela completes without a gameplay exception");
    check(game.currentUI_ != nullptr &&
              game.currentUI_->screenId_ == uistate::SCREEN_NPC_GREETING &&
              game.currentUI_->title_ == "Favela Dralor",
          "the eastern camp NPC opens Favela's greeting");
    game.commandAction(UIWidget::cmdOk_, nullptr);
    check(game.currentUI_ == game.NPCChoicesUI_[2],
          "Favela's greeting advances to her choices");
    game.commandAction(UIWidget::cmdCancel_, nullptr);
    game.gameCanvas_->resume();

    player->giftPoints_ = (int16_t)40;
    player->dungeonId_ = 1;
    player->gridX_ = stormhold::NpcSystem::npcGridX_[6];
    player->gridY_ = (int8_t)(stormhold::NpcSystem::npcGridY_[6] + 1);
    player->refreshSurroundings();

    const int32_t before = (int32_t)game.ownedUiWidgets_.size();
    std::printf("before: %d widgets, wardenVisits=%d wardenStage=%d\n", before,
                (int)game.worldState().npcs.wardenVisits,
                (int)static_cast<stormhold::Extension &>(player->extension()).wardenStage_);

    for (int32_t n = 0; n < kTicks; ++n) {
        player->vitals_[2] = player->vitals_[3];
        player->vitals_[6] = player->vitals_[7];
        g_now += kTickMs;
        game.gameCanvas_->repaintEnabled_ = false;
        game.gameCanvas_->tick();
        for (int32_t guard = 0; guard < 8 && game.currentUI_ != nullptr; ++guard) {
            UIWidget *screen = game.currentUI_;
            game.commandAction(UIWidget::cmdOk_, nullptr);
            if (game.currentUI_ == screen) break;
        }
        game.gameCanvas_->resume();
    }

    const int32_t after = (int32_t)game.ownedUiWidgets_.size();
    const int32_t grown = after - before;
    std::printf("after %d ticks: %d widgets (+%d), wardenVisits=%d wardenStage=%d\n",
                kTicks, after, grown, (int)game.worldState().npcs.wardenVisits,
                (int)static_cast<stormhold::Extension &>(player->extension()).wardenStage_);

    check(grown <= 16, "standing beside the Warden does not allocate per tick");
    if (grown > 16) {
        std::printf("that is %.1f widgets per tick -- the gate never closes\n",
                    (double)grown / (double)kTicks);
    }

    if (failures == 0) std::printf("warden leak: OK\n");
    return failures == 0 ? 0 : 1;
}
