#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <string>

#include "src/common/platform/desktop.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/spells.hpp"
#include "src/stormhold/cus_image.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/stormhold/profile.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/stormhold/variant.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/render/frame_check.hpp"
#include "src/common/render/render.hpp"
#include "src/stormhold/render/widescreen_scene.hpp"
#include "src/stormhold/render/raycast_scene.hpp"

using namespace stormhold;

namespace {

Game *g_game = nullptr;

int32_t g_pendingJob = 0;
bool g_hasPendingJob = false;

void queueJob(Game *, int32_t job) {
    g_pendingJob = job;
    g_hasPendingJob = true;
}

void drainPendingJob() {
    while (g_hasPendingJob) {
        g_hasPendingJob = false;
        g_game->runHelperJob(g_pendingJob);
    }
}
void noCanvasThread(GameCanvas *canvas) { canvas->running_ = true; }

void noSplashThread(UIWidget *) {}

int64_t g_now = 0;
int64_t g_splashAccumMs = 0;

int64_t captureClock() { return g_now; }

void captureSleep(int64_t ms) {
    if (ms > 0) g_now += ms;
}

void tickOnce() {
    if (g_game == nullptr) return;
    drainPendingJob();
    g_now += 250;
    if (g_game->splashUI_ != nullptr) {
        g_splashAccumMs += 250;
        if (g_splashAccumMs >= 500) {
            g_splashAccumMs -= 500;
            g_game->splashUI_->splashStep(500);
        }
    }
    if (g_game->gameCanvas_ != nullptr && g_game->gameCanvas_->running_) {
        g_game->gameCanvas_->tick();
    }
}

void settle(int ms) {
    int ticks = ms / 250;
    if (ticks < 1) ticks = 1;
    for (int n1 = 0; n1 < ticks; ++n1) tickOnce();
}

render::Surface g_screen(widescreen::kWideWidth, widescreen::kHeight);
render::Context g_renderer(&g_screen);
int g_failures = 0;

const int32_t KEY_SOFT_LEFT = -6;
const int32_t KEY_SOFT_RIGHT = -7;
const int32_t KEY_DOWN = -2;

render::Surface g_previous;

std::string g_baselineDir;
int g_mismatches = 0;

double snapshot(const std::string &dir, const char *name) {
    render::Surface copy;
    {
        std::lock_guard<std::mutex> guard(g_renderer.frameMutex());
        copy = g_screen;
    }
    size_t nonBlack = 0;
    size_t changed = 0;
    bool comparable = g_previous.width == copy.width && g_previous.height == copy.height;
    for (size_t n1 = 0; n1 < copy.pixels.size(); ++n1) {
        if ((copy.pixels[n1] & 0x00FFFFFFu) != 0) ++nonBlack;
        if (comparable && copy.pixels[n1] != g_previous.pixels[n1]) ++changed;
    }
    bool ok = render::writePng(copy, dir + "/" + name + ".png");
    double changedPct = comparable ? 100.0 * (double)changed / (double)copy.pixels.size() : 0.0;
    std::string verdict;
    if (!g_baselineDir.empty()) {
        framecheck::Result check = framecheck::compare(copy, g_baselineDir, name, dir);
        verdict = "  " + framecheck::describe(check);
        if (check.compared && !check.match) {
            ++g_mismatches;
        } else if (!check.compared) {
            ++g_mismatches;
        }
    }
    std::printf("  %-18s %s  %5.1f%% non-black  %5.1f%% changed%s\n", name,
                ok ? "wrote" : "FAILED",
                100.0 * (double)nonBlack / (double)copy.pixels.size(), changedPct,
                verdict.c_str());
    std::fflush(stdout);
    g_previous = copy;
    return changedPct;
}

void checkClipResetBetweenFrames() {
    g_renderer.beginPaint();
    render::SoftGraphics *graphics = g_renderer.screen();
    int32_t savedOrigin = graphics->originX();
    graphics->setOrigin(0, 0);
    graphics->setColor(0xFF00FF);
    graphics->fillRect(0, 0, g_screen.width, g_screen.height);
    size_t filled = 0;
    for (uint32_t pixel : g_screen.pixels) {
        if ((pixel & 0x00FFFFFFu) == 0xFF00FFu) ++filled;
    }
    graphics->setColor(0);
    graphics->fillRect(0, 0, g_screen.width, g_screen.height);
    graphics->setOrigin(savedOrigin, 0);
    g_renderer.endPaint();

    size_t total = g_screen.pixels.size();
    if (filled != total) {
        std::printf("  FAIL: clip leaked between frames -- a full-screen clear reached "
                    "%zu of %zu pixels (%.1f%%)\n",
                    filled, total, 100.0 * (double)filled / (double)total);
        ++g_failures;
    } else {
        std::printf("  clip reset between frames: full-screen clear reached all %zu "
                    "pixels\n",
                    total);
    }
    std::fflush(stdout);
}

Canvas *currentCanvas(Game *game) {
    Canvas *canvas = dynamic_cast<Canvas *>(g_game->display_->getCurrent());
    return canvas != nullptr ? canvas : (Canvas *)game->gameCanvas_;
}

void press(Game *game, int32_t code, int settleMs) {
    Canvas *canvas = currentCanvas(game);
    if (canvas != nullptr) {
        canvas->keyPressed(code);
        canvas->keyReleased(code);
    }
    settle(settleMs);
}

bool waitForScreen(int32_t screen, int timeoutMs) {
    int budget = timeoutMs / 250;
    if (budget < 4) budget = 4;
    for (int n1 = 0; n1 < budget; ++n1) {
        if (g_game->currentUI_ != nullptr && g_game->currentUI_->screenId_ == screen) {
            settle(150);
            return true;
        }
        tickOnce();
    }
    return false;
}

bool expectScreen(const char *what, int32_t screen, int timeoutMs) {
    if (waitForScreen(screen, timeoutMs)) {
        return true;
    }
    int32_t actual = g_game->currentUI_ != nullptr ? g_game->currentUI_->screenId_ : -1;
    std::printf("  FAIL: expected %s (screen %d) but sat on screen %d\n", what,
                (int)screen, (int)actual);
    ++g_failures;
    return false;
}

Display *display() { return g_game->display_; }

bool completeNameForm(const std::string &name, int timeoutMs) {
    int64_t deadline = g_game->platformContext_->nowMillis() + timeoutMs;
    Form *form = nullptr;
    while (g_game->platformContext_->nowMillis() < deadline) {
        form = dynamic_cast<Form *>(display()->getCurrent());
        if (form != nullptr && form->size() > 0) break;
        tickOnce();
    }
    if (form == nullptr) {
        std::printf("  FAIL: name form never became current\n");
        ++g_failures;
        return false;
    }
    for (int32_t n1 = 0; n1 < form->size(); ++n1) {
        if (TextField *field = dynamic_cast<TextField *>(form->get(n1))) {
            field->setString(std::string(name));
        }
    }
    const std::vector<Command *> &commands = form->commands();
    if (form->listener() == nullptr || commands.empty()) {
        std::printf("  FAIL: name form has no command to fire\n");
        ++g_failures;
        return false;
    }
    form->listener()->commandAction(commands[0], form);
    return true;
}

}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: %s <resource-dir> <out-dir> [save-dir] "
                     "[--verify <baseline-dir>]\n",
                     argv[0]);
        return 2;
    }
    std::string outDir = argv[2];
    {
        std::error_code outDirError;
        std::filesystem::create_directories(outDir, outDirError);
    }
    std::string saveArg;
    for (int n1 = 3; n1 < argc; ++n1) {
        if (std::strcmp(argv[n1], "--verify") == 0 && n1 + 1 < argc) {
            g_baselineDir = argv[++n1];
        } else if (saveArg.empty()) {
            saveArg = argv[n1];
        }
    }
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot(saveArg.empty() ? std::string("saves/rms") : saveArg);
    platform::PlatformContext captureContext;
    captureContext.installFileSystem(platform::defaultContext()->fileSystem());
    captureContext.installSaveStore(platform::defaultContext()->saveStore());

    captureContext.installRenderServices(&g_renderer);
    Game *game = nullptr;
    stormhold_widescreen::Context widescreenContext(&g_renderer, &g_screen, &game);
    stormhold_raycast::Context raycastContext(&g_renderer, &g_screen, &game);
    stormhold_init_statics(&captureContext);
    g_now = 1000000000000LL;
    captureContext.installClock(captureClock);
    captureContext.installSleep(captureSleep);
    game = new Game(profile(), &captureContext);
    game->setExecutionHooks(queueJob, noCanvasThread, noSplashThread);
    g_game = game;
    game->startApplication();
    drainPendingJob();

    std::printf("boot\n");
    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::printf("  FAIL: appload did not complete\n");
        return 1;
    }
    game->splashUI_->requestRepaint();
    game->splashUI_->flushRepaints();
    snapshot(outDir, "01_splash");

    for (int n1 = 0; n1 < 40 && !g_game->showCarrierLogo_; ++n1) {
        tickOnce();
    }
    if (!g_game->showCarrierLogo_) {
        std::printf("  FAIL: carrier logo screen never appeared\n");
        return 1;
    }
    game->splashUI_->requestRepaint();
    game->splashUI_->flushRepaints();
    snapshot(outDir, "01b_copyright");

    std::printf("menu flow\n");
    if (!expectScreen("main menu", 2, 30000)) return 1;
    snapshot(outDir, "02_main_menu");

    press(game, KEY_SOFT_RIGHT, 500);
    if (!expectScreen("class list", 3, 60000)) return 1;
    snapshot(outDir, "03_class_list");

    press(game, KEY_SOFT_RIGHT, 500);
    if (!expectScreen("character menu", 4, 8000)) return 1;
    snapshot(outDir, "04_character");

    press(game, KEY_DOWN, 300);
    press(game, KEY_SOFT_RIGHT, 500);
    if (!expectScreen("created prompt", 6, 8000)) return 1;

    press(game, KEY_SOFT_RIGHT, 500);
    if (!completeNameForm("Tester", 8000)) return 1;
    std::printf("  name form accepted\n");

    if (!expectScreen("welcome", 7, 30000)) return 1;
    snapshot(outDir, "05_welcome");

    press(game, KEY_SOFT_RIGHT, 500);
    if (!expectScreen("introduction", 101, 8000)) return 1;
    snapshot(outDir, "06_intro");

    press(game, KEY_SOFT_RIGHT, 1500);
    if (g_game->currentUI_ != nullptr) {
        std::printf("  FAIL: still on a menu after entering the game\n");
        ++g_failures;
    }
    std::printf("gameplay\n");
    snapshot(outDir, "07_dungeon");

    press(game, 50, 900);
    snapshot(outDir, "08_forward");

    press(game, 54, 900);
    snapshot(outDir, "09_strafed");

    press(game, -4, 900);
    snapshot(outDir, "10_turned");

    press(game, -4, 900);
    snapshot(outDir, "11_turned_again");

    std::printf("render invariants\n");
    checkClipResetBetweenFrames();
    settle(600);

    press(game, 42, 700);
    snapshot(outDir, "12_map");

    press(game, 55, 900);
    if (!expectScreen("options", 31, 8000)) return 1;
    snapshot(outDir, "13_options");

    game->currentUI_->setSelectedIndex(menuaction::rowOf(game->profile().optionsRows,
                                                         game->profile().optionsRowCount,
                                                         menuaction::PORT_OPTIONS));
    press(game, KEY_SOFT_RIGHT, 300);
    if (!expectScreen("display-controls", uistate::SCREEN_PORT_OPTIONS, 8000)) return 1;
    snapshot(outDir, "13b_display_controls");
    press(game, KEY_SOFT_LEFT, 300);
    game->currentUI_->setSelectedIndex(0);

    press(game, KEY_SOFT_LEFT, 900);
    if (g_game->currentUI_ != nullptr) {
        std::printf("  FAIL: back key did not leave the options menu\n");
        ++g_failures;
    }
    snapshot(outDir, "14_back_in_game");

    press(game, 48, 700);
    snapshot(outDir, "15_camping");

    int64_t campDeadline = g_game->platformContext_->nowMillis() + 20000;
    while (g_game->platformContext_->nowMillis() < campDeadline && game->gameCanvas_->campState_ != 0) {
        tickOnce();
    }
    if (game->gameCanvas_->campState_ != 0) {
        std::printf("  FAIL: camping never ended\n");
        ++g_failures;
    }
    settle(800);
    snapshot(outDir, "16_after_camp");

    std::printf("widescreen\n");
    widescreenContext.setEnabled(true);
    press(game, -4, 900);
    snapshot(outDir, "17_wide_turned");
    press(game, 50, 900);
    snapshot(outDir, "18_wide_forward");
    press(game, 55, 900);
    snapshot(outDir, "19_wide_menu");
    press(game, KEY_SOFT_LEFT, 800);
    press(game, 42, 700);
    snapshot(outDir, "20_wide_map");

    std::printf("3d\n");
    raycastContext.setEnabled(true);
    widescreenContext.setSceneOwnedExternally(true);
    press(game, -4, 900);
    snapshot(outDir, "21_3d_dungeon");
    press(game, 50, 900);
    snapshot(outDir, "22_3d_forward");
    press(game, -3, 900);
    snapshot(outDir, "23_3d_turned");

    {
        // Blindness must keep the 3D view: walls still cast, only the floor drops to black.
        Player *blinded = game->gameCanvas_->player_;
        const int8_t saved = blinded->ailments_;
        blinded->ailments_ = (int8_t)(saved | (1 << 2));
        press(game, -4, 900);
        snapshot(outDir, "23b_3d_blind");
        blinded->ailments_ = saved;
        press(game, -4, 900);
    }

    {
        raycastContext.setEnabled(false);
        widescreenContext.setSceneOwnedExternally(false);
        UIWidget *menu = new UIWidget(game, uistate::LAYOUT_FORM_1, 900);
        SharedArray<std::string> rows(3);
        rows[0] = std::string("First entry");
        rows[1] = std::string("Second entry");
        rows[2] = std::string("Third entry");
        menu->setupForm(std::string("Rows"), std::string("Body text"), rows);
        menu->setSelectedIndex(1);
        UIWidget *savedCurrent = g_game->currentUI_;
        g_game->currentUI_ = menu;
        g_game->uiCanvas_->widget_ = menu;
        menu->requestRepaint();
        menu->flushRepaints();
        snapshot(outDir, "24_menu_rows");
        g_game->currentUI_ = savedCurrent;
    }

    if (g_failures != 0 || g_mismatches != 0) {
        if (g_failures != 0) {
            std::printf("\n%d step(s) failed\n", g_failures);
        }
        if (g_mismatches != 0) {
            std::printf("\n%d frame(s) differ from the baseline in %s\n", g_mismatches,
                        g_baselineDir.c_str());
            std::printf("  each wrote <name>.actual.png and <name>.diff.png beside the "
                        "captured frames\n");
        }
        std::fflush(stdout);
        std::_Exit(1);
    }
    if (!g_baselineDir.empty()) {
        std::printf("\nplay-through OK, every frame matches the baseline\n");
    } else {
        std::printf("\nplay-through OK\n");
    }
    std::fflush(stdout);
    std::_Exit(0);
}
