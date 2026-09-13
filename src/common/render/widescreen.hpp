#ifndef COMMON_WIDESCREEN_HPP
#define COMMON_WIDESCREEN_HPP

#include "src/common/render/render.hpp"

namespace widescreen {

const int32_t kNarrowWidth = 176;
const int32_t kWideWidth = 370;
const int32_t kHeight = 208;

int32_t originX();

struct SideArt {
    double slope = 1.0;
    double offset = 0.0;
    int32_t fitAtlasA = 0;
    int32_t fitAtlasB = 0;
    int32_t tileBase = 0;
    int32_t tileDir = 1;

    double screenOf(int32_t atlas) const { return slope * (double)atlas + offset; }
};

struct Scene {
    bool dungeonVisible = false;
    bool blind = false;
    bool wash = false;
    int32_t washColor = 0;
    int32_t washHeight = kHeight;

    Image *floor = nullptr;
    Image *wallAtlas = nullptr;
    SideArt left;
    SideArt right;

    int32_t (*nearestSideWall)(void *ctx, bool leftSide, bool *authored) = nullptr;
    void (*drawSlice)(void *ctx, render::SoftGraphics *graphics, int32_t depth, bool leftSide,
                      int32_t x) = nullptr;
    void *ctx = nullptr;
};

typedef bool (*SceneProvider)(void *context, Scene *scene);

struct ProjectionRamp {
    double topA = 0.0, topB = 0.0;
    double botA = 0.0, botB = 0.0;
    bool valid = false;
};

class Context {
public:
    Context(render::Context *renderer, render::Surface *screen, SceneProvider provider,
            void *providerContext = nullptr);

    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }
    void setSceneOwnedExternally(bool owned) { sceneOwnedExternally_ = owned; }

    render::Context *renderer_;
    render::Surface *screen_;
    SceneProvider provider_;
    void *providerContext_;
    bool enabled_ = false;
    bool sceneOwnedExternally_ = false;
    ProjectionRamp left_;
    ProjectionRamp right_;
    const render::Surface *measured_ = nullptr;

private:
    static void postPaintHook(void *context);
    void postPaint();
};

}

#endif
