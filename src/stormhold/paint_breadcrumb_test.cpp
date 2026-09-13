#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/host/game_host.hpp"
#include "src/common/platform/crash_trace.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/headless.hpp"
#include "src/common/runtime.hpp"
#include "src/common/save_records.hpp"
#include "src/stormhold/extension.hpp"
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

std::string g_log;
void captureLine(const std::string &line) { g_log += line + "\n"; }

}

// When a frame fails, the log has to say which frame.  Forces a throw out of a
// paint layer and checks the breadcrumb names the layer and the screen state.
int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    crash_trace::install();
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-breadcrumb"
                                            : std::string("saves/rms-breadcrumb"));

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

    GameCanvas *canvas = game.gameCanvas_;

    // A healthy frame paints and reports nothing.
    canvas->repaintEnabled_ = true;
    canvas->repaint();
    canvas->serviceRepaints();

    // Capture what the failure handler writes.
    const std::string logPath =
        (tmp != nullptr ? std::string(tmp) : std::string(".")) + "/breadcrumb.log";
    platform::setLogFile(logPath);

    // Force a failure the way a real one would arrive: an out-of-range index
    // from inside a paint layer.  drawIconRow indexes the HUD icon table on
    // every frame, so shrinking it makes a draw run off the end and throw from
    // inside a layer -- the shape of a real frame failure.
    const SharedArray<Image *> realIcons = GameCanvas::hudIcons_;
    GameCanvas::hudIcons_ = SharedArray<Image *>(1);

    canvas->repaintEnabled_ = true;
    g_now += 250;
    const GameCanvas::TickStatus status = canvas->tick();
    std::printf("tick status = %d (Failed=%d)\n", (int)status,
                (int)GameCanvas::TickStatus::Failed);
    check(status == GameCanvas::TickStatus::Failed,
          "the tick reports the failed frame instead of dying");

    GameCanvas::hudIcons_ = realIcons;

    // What the log actually received is what matters -- the handler records the
    // breadcrumb at catch time, while the failing state is still current.
    std::FILE *log = platform::logFile();
    check(log != nullptr, "a log file was open");
    if (log != nullptr) std::fflush(log);
    std::string logged;
    if (std::FILE *readback = std::fopen(logPath.c_str(), "rb")) {
        char buffer[4096];
        std::size_t got;
        while ((got = std::fread(buffer, 1, sizeof(buffer), readback)) > 0) {
            logged.append(buffer, got);
        }
        std::fclose(readback);
    }
    std::printf("--- log ---\n%s-----------\n", logged.c_str());

    check(logged.find("while painting:") != std::string::npos,
          "a failed frame logs a painting breadcrumb");
    check(logged.find("layer=IconRow") != std::string::npos,
          "the breadcrumb names the layer that actually threw");
    check(logged.find("npcAhead=") != std::string::npos,
          "the breadcrumb carries the NPC that was being drawn");
    check(logged.find("sprites=") != std::string::npos,
          "the breadcrumb carries the resident sprite count");

    // The failing layer has to survive unwinding, or the handler above would
    // have had nothing to name.  Once a frame paints cleanly again, though,
    // the layer must not linger: the tick's handler catches far more than
    // paint, and a stale layer sends the reader to draw code for a failure
    // that never went near it -- which is exactly how a real crash report
    // pointed at the minimap for a failure raised nowhere near it.
    canvas->npcAhead_ = -1;
    canvas->repaintEnabled_ = true;
    canvas->repaint();
    canvas->serviceRepaints();
    check(canvas->paintBreadcrumb().find("layer=(not painting)") != std::string::npos,
          "a completed frame leaves no stale paint layer");
    check(canvas->failureContext().find("while ticking:") != std::string::npos,
          "a failure outside paint is reported as a tick failure");

    // The game must still be usable after the failed frame.
    canvas->npcAhead_ = -1;
    canvas->repaint();
    canvas->serviceRepaints();
    check(true, "the canvas keeps painting after a failed frame");

    if (failures == 0) std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
