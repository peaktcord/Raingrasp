#include <cstdio>
#include <stdexcept>

#include "src/common/ui.hpp"
#include "src/common/render/render.hpp"
#include "src/common/platform/platform.hpp"

namespace {

int failures = 0;

void check(bool value, const char *message) {
    if (!value) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

// A canvas whose paint() throws on demand, standing in for the game code that
// runs inside the frame lock.
class ThrowingCanvas : public Canvas {
public:
    bool throwOnPaint = false;
    int32_t paints = 0;

    explicit ThrowingCanvas(platform::PlatformContext *context) : Canvas(context) {}

    void paint(Graphics *) override {
        ++paints;
        if (throwOnPaint) {
            throw std::runtime_error("paint blew up");
        }
    }
};

}

// The frame lock is not recursive.  A paint that throws used to leak it, and
// the next paint on the same thread then died with
// resource_deadlock_would_occur -- thrown from inside the handler that was
// trying to draw the error screen, where nothing could catch it.
int main() {
    render::Surface screen(176, 208);
    render::Context renderer(&screen);

    platform::PlatformContext context;
    context.installRenderServices(&renderer);
    ThrowingCanvas canvas(&context);

    canvas.throwOnPaint = true;
    canvas.repaint();
    bool threw = false;
    try {
        canvas.serviceRepaints();
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "the failing paint propagates its own exception");
    check(canvas.paints == 1, "the failing paint ran");

    // The lock must be free again.  Before the fix this second paint threw
    // std::system_error instead of drawing.
    canvas.throwOnPaint = false;
    canvas.repaint();
    try {
        canvas.serviceRepaints();
    } catch (const std::system_error &e) {
        std::printf("FAIL: frame lock leaked: %s\n", e.what());
        ++failures;
    }
    check(canvas.paints == 2, "a later paint still runs after one throws");

    // And the lock is genuinely available to a fresh lock attempt.
    if (renderer.frameMutex().try_lock()) {
        renderer.frameMutex().unlock();
    } else {
        std::printf("FAIL: the frame mutex is still held\n");
        ++failures;
    }

    if (failures == 0) std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
