#include <cstdio>
#include <stdexcept>
#include <string>

#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/ui.hpp"

namespace {

int failures = 0;

void check(bool value, const char *message) {
    if (!value) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

// Mirrors what GameCanvas does: a paint that can throw, and a tick that catches
// the failure and then repaints to show an error screen.  That second repaint
// is the step that used to hit the poisoned lock.
class FakeGameCanvas : public Canvas {
public:
    bool failNextPaint = false;
    bool showingError = false;
    int32_t paints = 0;
    int32_t errorPaints = 0;

    explicit FakeGameCanvas(platform::PlatformContext *context) : Canvas(context) {}

    void paint(Graphics *) override {
        ++paints;
        if (showingError) {
            ++errorPaints;
            return;
        }
        if (failNextPaint) {
            failNextPaint = false;
            throw std::runtime_error("something in the frame blew up");
        }
    }

    // The shape of GameCanvas::tick's body plus its catch handlers.
    std::string tick() {
        try {
            this->repaint();
            this->serviceRepaints();
            return "ran";
        } catch (const std::exception &e) {
            const std::string what = e.what();
            showingError = true;
            this->showErrorScreen();
            return std::string("failed: ") + what;
        }
    }

    void showErrorScreen() {
        try {
            this->repaint();
            this->serviceRepaints();
        } catch (const std::exception &nested) {
            std::printf("   (error screen itself failed: %s)\n", nested.what());
        }
    }
};

}

// Does a failing frame leave the game recoverable, or does it take the process
// with it?  The distinction matters: the mutex fix is only worthwhile if the
// underlying failure was survivable in the first place.
int main() {
    render::Surface screen(176, 208);
    render::Context renderer(&screen);
    platform::PlatformContext context;
    context.installRenderServices(&renderer);

    FakeGameCanvas canvas(&context);

    check(canvas.tick() == "ran", "a healthy frame ticks normally");

    canvas.failNextPaint = true;
    const std::string result = canvas.tick();
    std::printf("tick reported: %s\n", result.c_str());
    check(result == "failed: something in the frame blew up",
          "the tick reports the original failure, not a mutex error");
    check(canvas.errorPaints == 1, "the error screen was drawn");

    // The real question: is the renderer still usable afterwards?
    canvas.showingError = false;
    check(canvas.tick() == "ran", "the game keeps ticking after a failed frame");

    if (renderer.frameMutex().try_lock()) {
        renderer.frameMutex().unlock();
    } else {
        std::printf("FAIL: the frame mutex is still held\n");
        ++failures;
    }

    std::printf("paints=%d errorPaints=%d\n", canvas.paints, canvas.errorPaints);
    if (failures == 0) std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
