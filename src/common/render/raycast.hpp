#ifndef COMMON_RAYCAST_HPP
#define COMMON_RAYCAST_HPP

#include "src/common/render/render.hpp"

namespace raycast {

struct Camera {
    double fx = 144.0;
    double fy = 119.5;
    double horizon = 36.0;
    double eyeToCeiling = 0.236;
    double eyeToFloor = 0.910;
};

struct WallTexture {
    Image *atlas = nullptr;
    int32_t x = 0, y = 0, w = 0, h = 0;
    double repeat = 4.0;
};

struct Scene {
    bool visible = false;
    // Blindness suppresses the floor/backdrop only, matching the narrow 2D view; walls and
    // geometry still render at full draw distance.
    bool blind = false;

    bool (*isWall)(void *ctx, int32_t mapX, int32_t mapY) = nullptr;
    void *ctx = nullptr;
    int32_t playerX = 0;
    int32_t playerY = 0;
    int32_t facing = 1;

    Image *backdrop = nullptr;
    bool flipBackdrop = false;
    WallTexture wall;
    Camera camera;

    double maxDistance = 3.5;
    Image *palette = nullptr;
    int32_t transparentKey = 0x0000FF;
};

typedef bool (*SceneProvider)(void *context, Scene *scene);

class Context {
public:
    Context(render::Context *renderer, render::Surface *screen, SceneProvider provider,
            void *providerContext = nullptr);

    void setEnabled(bool on);
    bool enabled() const { return enabled_; }
    bool ownsCurrentFrame() const;

    render::Context *renderer_;
    render::Surface *screen_;
    SceneProvider provider_;
    void *providerContext_;
    bool enabled_ = false;
    uint64_t drawnFrame_ = ~0ull;
    std::vector<double> fog_;
    const render::Surface *fogSource_ = nullptr;
    double fogRange_ = 0.0;
    std::vector<uint32_t> palette_;
    const render::Surface *paletteSource_ = nullptr;
    std::vector<uint8_t> paletteLut_;

private:
    static bool imageHook(void *context, render::SoftGraphics *graphics,
                          Image *image, int32_t x, int32_t y, int32_t anchor);
};

}

#endif
