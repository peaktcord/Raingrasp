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


// Which portrait sprites are resident for each NPC, and does the greeting draw
// survive?  Walks the player into a non-camp dungeon (which unloads every
// monster image and reloads only the bands that dungeon needs) and back, then
// reports the sprite slots drawNpcPortrait would dereference.
int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    crash_trace::install();
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-portrait"
                                            : std::string("saves/rms-portrait"));

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

    auto dumpSlots = [&](const char *when) {
        std::printf("-- %s: npcSprites_ slots --\n", when);
        int32_t nulls = 0;
        for (int32_t i = 0; i < GameCanvas::npcSprites_.length(); ++i) {
            if (GameCanvas::npcSprites_[i] == nullptr) ++nulls;
        }
        std::printf("   %d of %d slots are null\n", nulls,
                    GameCanvas::npcSprites_.length());
        // The slots drawNpcPortrait reaches for, per NPC (band -> layout[4]).
        const int32_t portraitBand[7] = {1, 6, 7, 2, 3, 8, 0};
        for (int32_t npc = 0; npc < 6; ++npc) {
            const int32_t band = portraitBand[npc];
            const game::SpriteBand *b = game.gameCanvas_->spriteBandOf(band);
            std::printf("   npc %d band %d -> row %d\n", npc, band,
                        b != nullptr ? b->row : -1);
        }
    };

    dumpSlots("in camp after creation");

    // Leave the camp for a real dungeon, which unloads every monster image and
    // reloads only what that dungeon's monsters need.
    game.character_->dungeonId_ = 2;
    game.loadingDungeonId_ = 2;
    game.runImageLoader();
    dumpSlots("after loading dungeon 2");

    // Draw a portrait with the sprites unloaded, which is what the canvas does
    // when it paints the NPC portrait layer for whoever is standing ahead.
    Graphics *graphics = renderer.screenGraphics();
    for (int32_t npc = 0; npc < 7; ++npc) {
        stormhold::ext(game.character_).drawNpcPortrait(*game.gameCanvas_, graphics, npc);
    }
    check(true, "every NPC portrait draws with the monster art unloaded");

    // The far/mid helpers are handed -1 when spriteBandOf finds no band.
    game.gameCanvas_->drawMonsterFar(graphics, -1, 8);
    game.gameCanvas_->drawMonsterMid(graphics, -1, 4);
    check(true, "an absent sprite band draws nothing instead of crashing");

    if (failures == 0) std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
