#include "src/common/render/raycast.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace raycast {

namespace {

const int32_t kBackdropTile = 36;
const int32_t kMaxSteps = 64;

const int kFogSteps = 96;
double rowLuma(const render::Surface *surface, int32_t y) {
    if (y < 0) y = 0;
    if (y >= surface->height) y = surface->height - 1;
    double total = 0.0;
    int32_t n = 0;
    const uint32_t *row = surface->row(y);
    for (int32_t x = 0; x < surface->width; ++x) {
        uint32_t argb = row[x];
        if ((argb >> 24) == 0) continue;
        total += 0.299 * ((argb >> 16) & 0xFF) + 0.587 * ((argb >> 8) & 0xFF) +
                 0.114 * (argb & 0xFF);
        ++n;
    }
    return n > 0 ? total / n : 0.0;
}

void buildFog(Context *context, const render::Surface *backdrop, const Camera &cam,
              double maxDistance) {
    if (backdrop == context->fogSource_ && context->fogRange_ == maxDistance) {
        return;
    }
    context->fogSource_ = backdrop;
    context->fogRange_ = maxDistance;
    context->fog_.assign(kFogSteps + 1, 1.0);
    if (backdrop == nullptr || backdrop->empty() || maxDistance <= 0.0) {
        return;
    }
    auto lumaAt = [&](double z) {
        if (z < 0.05) z = 0.05;
        double y = cam.horizon + cam.fy * cam.eyeToFloor / z;
        return rowLuma(backdrop, (int32_t)(y + 0.5));
    };
    double reference = lumaAt(1.0);
    if (reference <= 1.0) {
        return;
    }
    for (int i = 0; i <= kFogSteps; ++i) {
        double z = maxDistance * (double)i / (double)kFogSteps;
        double f = lumaAt(z) / reference;
        context->fog_[(size_t)i] = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
    }
}

double fogAt(Context *context, double z, double maxDistance) {
    if (context->fog_.empty() || maxDistance <= 0.0) return 1.0;
    double t = z / maxDistance * (double)kFogSteps;
    if (t <= 0.0) return context->fog_[0];
    if (t >= (double)kFogSteps) return context->fog_[(size_t)kFogSteps];
    int i = (int)t;
    double frac = t - (double)i;
    return context->fog_[(size_t)i] * (1.0 - frac) +
           context->fog_[(size_t)i + 1] * frac;
}

void buildPalette(Context *context, const render::Surface *source, int32_t transparentKey) {
    if (source == context->paletteSource_) {
        return;
    }
    context->paletteSource_ = source;
    context->palette_.clear();
    context->paletteLut_.clear();
    if (source == nullptr || source->empty()) {
        return;
    }
    for (int32_t y = 0; y < source->height; ++y) {
        const uint32_t *row = source->row(y);
        for (int32_t x = 0; x < source->width; ++x) {
            uint32_t argb = row[x];
            if ((argb >> 24) == 0) continue;
            uint32_t rgb = argb & 0x00FFFFFFu;
            if (rgb == (uint32_t)transparentKey) continue;
            if (std::find(context->palette_.begin(), context->palette_.end(), rgb) ==
                context->palette_.end()) {
                context->palette_.push_back(rgb);
                if (context->palette_.size() >= 256) break;
            }
        }
    }
    if (context->palette_.empty()) {
        return;
    }
    context->paletteLut_.resize(32 * 32 * 32);
    for (int32_t r = 0; r < 32; ++r) {
        for (int32_t g = 0; g < 32; ++g) {
            for (int32_t b = 0; b < 32; ++b) {
                int32_t rr = (r << 3) | (r >> 2);
                int32_t gg = (g << 3) | (g >> 2);
                int32_t bb = (b << 3) | (b >> 2);
                int32_t best = 0;
                long bestDist = 1L << 30;
                for (size_t n = 0; n < context->palette_.size(); ++n) {
                    long dr = (long)rr - (long)((context->palette_[n] >> 16) & 0xFF);
                    long dg = (long)gg - (long)((context->palette_[n] >> 8) & 0xFF);
                    long db = (long)bb - (long)(context->palette_[n] & 0xFF);
                    long dist = 3 * dr * dr + 6 * dg * dg + db * db;
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = (int32_t)n;
                    }
                }
                context->paletteLut_[(size_t)((r << 10) | (g << 5) | b)] =
                    (uint8_t)best;
            }
        }
    }
}

uint32_t quantise(Context *context, uint32_t rgb) {
    if (context->paletteLut_.empty()) {
        return rgb;
    }
    int32_t r = (int32_t)((rgb >> 19) & 0x1F);
    int32_t g = (int32_t)((rgb >> 11) & 0x1F);
    int32_t b = (int32_t)((rgb >> 3) & 0x1F);
    return context->palette_[context->paletteLut_[(size_t)((r << 10) | (g << 5) | b)]];
}

void clearSurface(render::Surface *dst) {
    for (int32_t y = 0; y < dst->height; ++y) {
        uint32_t *row = dst->row(y);
        for (int32_t x = 0; x < dst->width; ++x) {
            row[x] = 0xFF000000u;
        }
    }
}

void drawBackdrop(render::Surface *dst, Image *backdrop, int32_t originX, bool flip) {
    render::Surface *tile = render::surfaceFor(backdrop);
    if (tile == nullptr || tile->empty()) {
        return;
    }
    int32_t first = originX % kBackdropTile;
    if (first > 0) first -= kBackdropTile;
    for (int32_t y = 0; y < dst->height; ++y) {
        int32_t band = y / tile->height;
        int32_t ty = y % tile->height;
        if ((band & 1) != 0) {
            ty = tile->height - 1 - ty;
        }
        const uint32_t *src = tile->row(ty);
        uint32_t *out = dst->row(y);
        for (int32_t x0 = first; x0 < dst->width; x0 += kBackdropTile) {
            for (int32_t tx = 0; tx < tile->width; ++tx) {
                int32_t x = x0 + tx;
                if (x < 0 || x >= dst->width) continue;
                uint32_t argb = src[flip ? tile->width - 1 - tx : tx];
                if ((argb >> 24) == 0) continue;
                out[x] = 0xFF000000u | (argb & 0x00FFFFFFu);
            }
        }
    }
}

struct Hit {
    bool found = false;
    double distance = 0.0;
    double along = 0.0;
};

Hit cast(const Scene &scene, double originX, double originY, double dirX, double dirY,
         double maxDistance) {
    Hit hit;
    if (dirX == 0.0 && dirY == 0.0) {
        return hit;
    }
    int32_t mapX = (int32_t)std::floor(originX);
    int32_t mapY = (int32_t)std::floor(originY);
    double deltaX = dirX == 0.0 ? 1e30 : std::fabs(1.0 / dirX);
    double deltaY = dirY == 0.0 ? 1e30 : std::fabs(1.0 / dirY);
    int32_t stepX = dirX < 0 ? -1 : 1;
    int32_t stepY = dirY < 0 ? -1 : 1;
    double sideX = dirX < 0 ? (originX - mapX) * deltaX : (mapX + 1.0 - originX) * deltaX;
    double sideY = dirY < 0 ? (originY - mapY) * deltaY : (mapY + 1.0 - originY) * deltaY;

    bool vertical = false;
    for (int32_t step = 0; step < kMaxSteps; ++step) {
        if (std::min(sideX, sideY) > maxDistance + 1.0) {
            return hit;
        }
        if (sideX < sideY) {
            sideX += deltaX;
            mapX += stepX;
            vertical = true;
        } else {
            sideY += deltaY;
            mapY += stepY;
            vertical = false;
        }
        if (scene.isWall(scene.ctx, mapX, mapY)) {
            hit.found = true;
            if (vertical) {
                hit.distance = (mapX - originX + (1 - stepX) / 2.0) / dirX;
                hit.along = originY + hit.distance * dirY;
            } else {
                hit.distance = (mapY - originY + (1 - stepY) / 2.0) / dirY;
                hit.along = originX + hit.distance * dirX;
            }
            hit.along -= std::floor(hit.along);
            return hit;
        }
    }
    return hit;
}

void renderScene(Context *context, render::Surface *dst, const Scene &scene, int32_t originX) {
    render::Surface *tex = render::surfaceFor(scene.wall.atlas);
    if (tex == nullptr || tex->empty()) {
        return;
    }
    buildPalette(context, render::surfaceFor(scene.palette != nullptr ? scene.palette
                                                                      : scene.wall.atlas),
                 scene.transparentKey);
    buildFog(context, render::surfaceFor(scene.backdrop), scene.camera, scene.maxDistance);

    double forwardX = 0.0, forwardY = 0.0;
    switch (scene.facing) {
        case 1: forwardY = -1.0; break;
        case 2: forwardX = 1.0; break;
        case 3: forwardY = 1.0; break;
        default: forwardX = -1.0; break;
    }
    double rightX = -forwardY;
    double rightY = forwardX;

    double eyeX = scene.playerX + 0.5;
    double eyeY = scene.playerY + 0.5;
    const Camera &cam = scene.camera;
    double centreX = (double)originX + 90.0;

    for (int32_t sx = 0; sx < dst->width; ++sx) {
        double cameraX = ((double)sx + 0.5 - centreX) / cam.fx;
        double dirX = forwardX + rightX * cameraX;
        double dirY = forwardY + rightY * cameraX;
        Hit hit = cast(scene, eyeX, eyeY, dirX, dirY, scene.maxDistance);
        if (!hit.found || hit.distance <= 0.01 || hit.distance > scene.maxDistance) {
            continue;
        }

        double top = cam.horizon - cam.fy * cam.eyeToCeiling / hit.distance;
        double bottom = cam.horizon + cam.fy * cam.eyeToFloor / hit.distance;
        if (bottom <= 0.0 || top >= (double)dst->height) {
            continue;
        }
        double fog = fogAt(context, hit.distance, scene.maxDistance);

        double u = hit.along * scene.wall.repeat;
        u -= std::floor(u);
        int32_t texX = scene.wall.x + (int32_t)(u * (double)scene.wall.w);
        if (texX < scene.wall.x) texX = scene.wall.x;
        if (texX >= scene.wall.x + scene.wall.w) texX = scene.wall.x + scene.wall.w - 1;

        int32_t y0 = (int32_t)std::max(0.0, std::floor(top));
        int32_t y1 = (int32_t)std::min((double)dst->height, std::ceil(bottom));
        uint32_t *out = dst->row(0);
        for (int32_t y = y0; y < y1; ++y) {
            double v = ((double)y + 0.5 - top) / (bottom - top);
            int32_t texY = scene.wall.y + (int32_t)(v * (double)scene.wall.h);
            if (texY < scene.wall.y) texY = scene.wall.y;
            if (texY >= scene.wall.y + scene.wall.h) texY = scene.wall.y + scene.wall.h - 1;
            uint32_t argb = tex->row(texY)[texX];
            if ((argb >> 24) == 0) continue;
            uint32_t r = (uint32_t)(((argb >> 16) & 0xFF) * fog);
            uint32_t g = (uint32_t)(((argb >> 8) & 0xFF) * fog);
            uint32_t b = (uint32_t)((argb & 0xFF) * fog);
            out[(size_t)y * (size_t)dst->width + (size_t)sx] =
                0xFF000000u | quantise(context, (r << 16) | (g << 8) | b);
        }
    }
}

}

bool Context::imageHook(void *opaque, render::SoftGraphics *graphics, Image *image,
                        int32_t, int32_t, int32_t) {
    Context *context = static_cast<Context *>(opaque);
    (void)graphics;
    if (!context->enabled_ || context->screen_ == nullptr ||
        context->provider_ == nullptr || image == nullptr) {
        return false;
    }
    Scene scene;
    if (!context->provider_(context->providerContext_, &scene) || !scene.visible) {
        return false;
    }
    const bool trigger = scene.blind || image == scene.backdrop || image == scene.wall.atlas;
    if (!trigger) {
        return false;
    }
    if (context->drawnFrame_ != context->renderer_->frameSerial()) {
        context->drawnFrame_ = context->renderer_->frameSerial();
        int32_t originX = graphics->originX();
        clearSurface(context->screen_);
        if (!scene.blind) {
            drawBackdrop(context->screen_, scene.backdrop, originX, scene.flipBackdrop);
        }
        renderScene(context, context->screen_, scene, originX);
    }
    return image == scene.backdrop || image == scene.wall.atlas;
}

Context::Context(render::Context *renderer, render::Surface *screen, SceneProvider provider,
                 void *providerContext)
    : renderer_(renderer), screen_(screen), provider_(provider),
      providerContext_(providerContext) {
    renderer->setImageHook(&Context::imageHook, this);
}

void Context::setEnabled(bool on) {
    enabled_ = on;
    drawnFrame_ = ~0ull;
}

bool Context::ownsCurrentFrame() const {
    return enabled_ && renderer_ != nullptr && drawnFrame_ == renderer_->frameSerial();
}

}
