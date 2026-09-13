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

// Talking to the Warden (NPC 6) used to index NPCChoicesUI_, an array of six
// entries, at [6].  The out-of-bounds read handed back whatever pointer
// followed the heap buffer and the greeting screen then wrote through it.
int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    crash_trace::install();
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-warden-talk"
                                            : std::string("saves/rms-warden-talk"));

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
    worldstate::NpcState &npcs = game.worldState().npcs;

    check(game.NPCChoicesUI_.length() == 6, "there are six NPC choice screens");
    std::printf("NPCChoicesUI_ holds %d entries; the Warden is NPC 6\n",
                game.NPCChoicesUI_.length());

    // Put the Warden in camp and stand the player next to him, which is the
    // state the player reaches once giftPoints crosses the first threshold.
    // Varus is in camp on his second visit and has not spoken his line yet, so
    // interact(.., 6, 1, 0) returns text and openNpcScreen takes the branch
    // that reaches for NPCChoicesUI_[6].
    npcs.wardenVisits = (int8_t)2;
    npcs.wardenPresent = true;
    static_cast<stormhold::Extension &>(player->extension()).wardenStage_ = (int16_t)1;
    player->giftPoints_ = (int16_t)40;
    player->dungeonId_ = 1;
    player->gridX_ = stormhold::NpcSystem::npcGridX_[6];
    player->gridY_ = (int8_t)(stormhold::NpcSystem::npcGridY_[6] + 1);
    player->refreshSurroundings();

    // This is exactly what GameCanvas::doTalk does once npcAhead_ resolves to
    // Varus.  Calling it directly keeps the world tick's warden handoff from
    // consuming the greeting before the talk path runs.
    game.gameCanvas_->npcAhead_ = 6;
    stormhold::variantOf(*player).openNpcScreen(*game.gameCanvas_, 6);

    // Varus speaks his line on his own screen rather than being routed through
    // a choices menu he does not have.
    check(game.currentUI_ != nullptr, "talking to the Warden opened a screen");
    if (game.currentUI_ == nullptr) return 1;
    std::printf("currentUI screenId=%d title=%s\n", game.currentUI_->screenId_,
                game.currentUI_->title_.c_str());
    check(game.currentUI_->screenId_ == stormhold::screens::RETURN_TO_GAME_PAUSED,
          "the Warden gets his own speaks screen, not an NPC choices menu");
    check(game.currentUI_->title_ == "Varus", "the screen is titled Varus");

    // Dismissing it hands the player back to the map.  This is the step that
    // used to dereference the out-of-bounds pointer.
    game.commandAction(UIWidget::cmdOk_, nullptr);
    check(true, "dismissing the Warden screen survived");

    if (failures == 0) std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
