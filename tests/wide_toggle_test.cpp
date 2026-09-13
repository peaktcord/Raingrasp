// `F11` may be pressed at any moment, including before a character exists.
//
// The toggle repaints immediately rather than waiting for the tick, because
// the window has already resized and the centre band would otherwise hold the
// previous view for several frames. That forced repaint used to go straight
// at game_->gameCanvas_, which is the wrong canvas whenever a menu is up: the
// game canvas exists from boot onward, so the repaint painted the dungeon view
// with no player to read (a null deref on the splash and the main menu) and,
// once a character existed, painted the dungeon on top of whatever menu was
// current. It has to repaint whatever the display says is *current* -- the
// menu canvas in the menus, the game canvas only during play.
//
// So this applies a wide setting at each stage the player can reach: the
// splash, the main menu, and mid-play. main_sdl.cpp is a binary with an SDL
// window, so the render half is driven through the same host seam the loop
// calls rather than through the loop.
#include <cstdio>
#include <memory>
#include <string>

#include "src/common/host/game_host.hpp"
#include "src/common/host/registry.hpp"
#include "src/common/game/binary_io.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/render/widescreen.hpp"

namespace {

int failures = 0;

void check(bool value, const char *message) {
    std::printf("%s %s\n", value ? "ok  " : "FAIL", message);
    if (!value) ++failures;
}

// The render half of main_sdl's settings reconciliation: switch the mode,
// then paint the current canvas into the new framing immediately.
void applyWideSetting(host::GameHost *game) {
    game->setWideView(!game->wideView());
    game->repaintCanvasNow();
}

bool menuSidesExtendCurrentRows(const render::Surface &screen) {
    const int32_t left = widescreen::originX();
    const int32_t right = left + widescreen::kNarrowWidth - 1;
    for (int32_t y = 0; y < screen.height; ++y) {
        const uint32_t *row = screen.row(y);
        for (int32_t x = 0; x < left; ++x) {
            if (row[x] != row[left]) return false;
        }
        for (int32_t x = right + 1; x < screen.width; ++x) {
            if (row[x] != row[right]) return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <game-id> <resource-dir>\n", argv[0]);
        return 2;
    }
    const std::string id = argv[1];
    std::unique_ptr<host::GameHost> game = host::createHost(id);
    if (game == nullptr) {
        std::fprintf(stderr, "no such game: %s\n", id.c_str());
        return 2;
    }

    Resources::setRoot(argv[2]);
    SaveRecordFiles::setRoot(std::string("saves/wide-toggle-test-") + id);

    // The wide surface the front end always allocates; narrow mode presents
    // its centre band rather than resizing it.
    render::Surface screen(widescreen::kWideWidth, widescreen::kHeight);
    render::Context renderer(&screen);
    platform::PlatformContext context;
    context.installFileSystem(platform::defaultContext()->fileSystem());
    context.installSaveStore(platform::defaultContext()->saveStore());
    context.installRenderServices(&renderer);

    game->boot(&context, &renderer, &screen);

    // On the splash: the game canvas already exists, but no player does.
    check(game->hasCanvas(), "the game canvas exists before the game starts");
    check(!game->canvasRunning(), "but it is not running yet");
    applyWideSetting(game.get());
    check(game->wideView(), "F11 on the splash turns the wide view on");
    applyWideSetting(game.get());
    check(!game->wideView(), "and a second press turns it off again");

    // Step the splash out to the main menu, and press it there too.
    for (int guard = 0; guard < 200; ++guard) {
        if (!game->stepSplash(500)) break;
    }
    game->drainPendingWork();
    check(!game->canvasRunning(), "the main menu is up, not the game");
    applyWideSetting(game.get());
    check(menuSidesExtendCurrentRows(screen),
          "a menu fills newly exposed wide rows on the first repaint");
    applyWideSetting(game.get());
    check(!game->wideView(), "F11 in the menus leaves the mode where it started");

    // Now into play, through the scripted flow the replay baseline uses, and
    // press it once the dungeon really is what is current.
    replay::Script script = game->replayScript();
    int lastTick = 0;
    for (const replay::Event &event : script.events) {
        while (lastTick < event.tick) {
            host::completeNameForm(game->display(), "Tester");
            game->drainPendingWork();
            if (game->canvasRunning()) {
                int64_t period = 0;
                game->tick(&period);
            }
            game->drainPendingWork();
            ++lastTick;
        }
        if (event.press) {
            game->keyPressed(event.key);
        } else {
            game->keyReleased(event.key);
        }
        game->drainPendingWork();
    }
    check(game->canvasRunning(), "the dungeon is running after the scripted flow");
    applyWideSetting(game.get());
    check(game->wideView(), "F11 in play turns the wide view on");
    applyWideSetting(game.get());
    check(!game->wideView(), "and off");

    std::printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
