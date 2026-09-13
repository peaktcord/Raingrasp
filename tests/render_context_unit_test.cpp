#include "src/common/platform/platform.hpp"
#include "src/common/render/raycast.hpp"
#include "src/common/render/render.hpp"
#include "src/common/render/widescreen.hpp"

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class PaintCanvas : public Canvas {
public:
    PaintCanvas(platform::PlatformContext *context, int32_t color)
        : Canvas(context), color_(color) {}

    void paint(Graphics *graphics) override {
        graphics->setColor(color_);
        graphics->fillRect(0, 0, 2, 2);
    }

private:
    int32_t color_;
};

void countPostPaint(void *context) { ++*static_cast<int *>(context); }

bool countImage(void *context, render::SoftGraphics *, Image *, int32_t, int32_t, int32_t) {
    ++*static_cast<int *>(context);
    return true;
}

bool countWideScene(void *context, widescreen::Scene *) {
    ++*static_cast<int *>(context);
    return false;
}

bool countRayScene(void *context, raycast::Scene *) {
    ++*static_cast<int *>(context);
    return false;
}

}  // namespace

int main() {
    platform::PlatformContext firstPlatform;
    platform::PlatformContext secondPlatform;
    render::Surface firstSurface(2, 2);
    render::Surface secondSurface(2, 2);
    render::Context firstRenderer(&firstSurface);
    render::Context secondRenderer(&secondSurface);
    firstPlatform.installRenderServices(&firstRenderer);
    secondPlatform.installRenderServices(&secondRenderer);

    int firstPostPaint = 0;
    int secondPostPaint = 0;
    firstRenderer.setPostPaint(countPostPaint, &firstPostPaint);
    secondRenderer.setPostPaint(countPostPaint, &secondPostPaint);

    PaintCanvas firstCanvas(&firstPlatform, 0x112233);
    PaintCanvas secondCanvas(&secondPlatform, 0x445566);
    firstCanvas.repaint();
    firstCanvas.serviceRepaints();
    check(firstSurface.pixels[0] == 0xFF112233u &&
              secondSurface.pixels[0] == 0xFF000000u,
          "first canvas paints only its session surface");
    check(firstRenderer.frameSerial() == 1 && secondRenderer.frameSerial() == 0 &&
              firstPostPaint == 1 && secondPostPaint == 0,
          "first paint advances only its session hooks and counter");

    secondCanvas.repaint();
    secondCanvas.serviceRepaints();
    check(secondSurface.pixels[0] == 0xFF445566u &&
              firstSurface.pixels[0] == 0xFF112233u,
          "second canvas paints only its session surface");
    check(firstRenderer.frameSerial() == 1 && secondRenderer.frameSerial() == 1 &&
              firstPostPaint == 1 && secondPostPaint == 1,
          "second paint leaves first-session hooks and counter unchanged");

    Image *firstImage = Image::createImage(&firstPlatform, 1, 1);
    Image *secondImage = Image::createImage(&secondPlatform, 1, 1);
    Graphics *firstImageGraphics = firstImage->getGraphics();
    Graphics *secondImageGraphics = secondImage->getGraphics();
    check(firstImageGraphics != secondImageGraphics,
          "mutable-image graphics caches are session-owned");

    int firstImages = 0;
    int secondImages = 0;
    firstRenderer.setImageHook(countImage, &firstImages);
    secondRenderer.setImageHook(countImage, &secondImages);
    firstRenderer.screen()->drawImage(firstImage, 0, 0, render::LEFT | render::TOP);
    check(firstImages == 1 && secondImages == 0,
          "image interception stays in the first renderer");
    secondRenderer.screen()->drawImage(secondImage, 0, 0, render::LEFT | render::TOP);
    check(firstImages == 1 && secondImages == 1,
          "image interception stays in the second renderer");

    render::Surface firstWideSurface(widescreen::kWideWidth, widescreen::kHeight);
    render::Surface secondWideSurface(widescreen::kWideWidth, widescreen::kHeight);
    render::Context firstWideRenderer(&firstWideSurface);
    render::Context secondWideRenderer(&secondWideSurface);
    int firstWideScenes = 0;
    int secondWideScenes = 0;
    widescreen::Context firstWide(&firstWideRenderer, &firstWideSurface,
                                  countWideScene, &firstWideScenes);
    widescreen::Context secondWide(&secondWideRenderer, &secondWideSurface,
                                   countWideScene, &secondWideScenes);
    firstWide.setEnabled(true);
    firstWideRenderer.beginPaint();
    firstWideRenderer.endPaint();
    check(firstWideScenes == 1 && secondWideScenes == 0 && firstWide.enabled() &&
              !secondWide.enabled(),
          "widescreen providers and mode flags are context-owned");

    render::Surface firstRaySurface(2, 2);
    render::Surface secondRaySurface(2, 2);
    render::Context firstRayRenderer(&firstRaySurface);
    render::Context secondRayRenderer(&secondRaySurface);
    int firstRayScenes = 0;
    int secondRayScenes = 0;
    raycast::Context firstRay(&firstRayRenderer, &firstRaySurface, countRayScene,
                              &firstRayScenes);
    raycast::Context secondRay(&secondRayRenderer, &secondRaySurface, countRayScene,
                               &secondRayScenes);
    firstRay.setEnabled(true);
    firstRayRenderer.screen()->drawImage(firstImage, 0, 0,
                                         render::LEFT | render::TOP);
    check(firstRayScenes == 1 && secondRayScenes == 0 && firstRay.enabled() &&
              !secondRay.enabled(),
          "raycast providers, caches, and mode flags are context-owned");

    delete firstImage;
    delete secondImage;
    return failures == 0 ? 0 : 1;
}
