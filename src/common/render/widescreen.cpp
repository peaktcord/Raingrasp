#include "src/common/render/widescreen.hpp"

#include <algorithm>
#include <cmath>

namespace widescreen {

namespace {

const int32_t kColumn = 18;
const int32_t kGameSpan = 180;
const int32_t kFloorTile = 36;

int32_t extraColumns() {
    return (originX() + kColumn - 1) / kColumn + 1;
}

bool columnExtent(const render::Surface *surface, int32_t x, int32_t *top, int32_t *bottom) {
    if (x < 0 || x >= surface->width) {
        return false;
    }
    int32_t first = -1;
    int32_t last = -1;
    for (int32_t y = 0; y < surface->height; ++y) {
        if ((surface->row(y)[x] >> 24) != 0) {
            if (first < 0) first = y;
            last = y;
        }
    }
    if (first < 0) return false;
    *top = first;
    *bottom = last;
    return true;
}

bool fitRamp(const render::Surface *wall, const SideArt &art, ProjectionRamp *ramp) {
    int32_t tA, bA, tB, bB;
    if (!columnExtent(wall, art.fitAtlasA, &tA, &bA) ||
        !columnExtent(wall, art.fitAtlasB, &tB, &bB)) {
        return false;
    }
    double screenA = art.screenOf(art.fitAtlasA);
    double screenB = art.screenOf(art.fitAtlasB);
    double span = screenB - screenA;
    if (span == 0.0) return false;
    ramp->topB = (double)(tB - tA) / span;
    ramp->botB = (double)(bB - bA) / span;
    ramp->topA = (double)tA - ramp->topB * screenA;
    ramp->botA = (double)bA - ramp->botB * screenA;
    ramp->valid = true;
    return true;
}

void measureRamps(Context *context, const render::Surface *wall, const Scene &scene) {
    if (context->measured_ == wall) {
        return;
    }
    context->measured_ = wall;
    context->left_.valid = false;
    context->right_.valid = false;
    if (wall == nullptr || wall->empty()) {
        return;
    }
    fitRamp(wall, scene.left, &context->left_);
    fitRamp(wall, scene.right, &context->right_);
}

bool fillPerspectiveColumn(Context *context, render::Surface *dst, const render::Surface *wall,
                           const SideArt &art, bool left, int32_t gameX, double scale) {
    const ProjectionRamp &ramp = left ? context->left_ : context->right_;
    if (!ramp.valid || dst == nullptr || wall == nullptr || wall->empty()) {
        return false;
    }
    int32_t absX = originX() + gameX;
    if (absX < 0 || absX >= dst->width) {
        return true;
    }

    int32_t step = left ? (-gameX - 1) % kColumn : (gameX - kGameSpan) % kColumn;
    int32_t sourceCol = art.tileBase + art.tileDir * step;
    if (sourceCol < 0 || sourceCol >= wall->width) {
        return false;
    }
    double sourceScreen = art.screenOf(sourceCol);

    double srcTop = ramp.topA + ramp.topB * sourceScreen;
    double srcBot = ramp.botA + ramp.botB * sourceScreen;
    double horizon = ramp.topA + ramp.topB * ((ramp.botA - ramp.topA) / (ramp.topB - ramp.botB));
    double dstTop = horizon + (ramp.topA + ramp.topB * (double)gameX - horizon) * scale;
    double dstBot = horizon + (ramp.botA + ramp.botB * (double)gameX - horizon) * scale;
    if (dstBot - dstTop < 1.0 || srcBot - srcTop < 1.0) {
        return false;
    }

    int32_t y0 = (int32_t)std::max(0.0, std::floor(dstTop));
    int32_t y1 = (int32_t)std::min((double)dst->height, std::ceil(dstBot));
    for (int32_t y = y0; y < y1; ++y) {
        double t = ((double)y + 0.5 - dstTop) / (dstBot - dstTop);
        double sy = srcTop + t * (srcBot - srcTop);
        int32_t srcY = (int32_t)(sy + 0.5);
        if (srcY < 0) srcY = 0;
        if (srcY >= wall->height) srcY = wall->height - 1;
        uint32_t argb = wall->row(srcY)[sourceCol];
        if ((argb >> 24) == 0) {
            continue;
        }
        dst->row(y)[absX] = 0xFF000000u | (argb & 0x00FFFFFFu);
    }
    return true;
}

void drawSide(Context *context, render::SoftGraphics *graphics, const Scene &scene, bool left) {
    const SideArt &art = left ? scene.left : scene.right;
    if (left) {
        graphics->setClip(-originX(), 0, originX(), kHeight);
    } else {
        graphics->setClip(kGameSpan, 0, context->screen_->width - originX() - kGameSpan, kHeight);
    }

    if (scene.wash) {
        graphics->setColor(scene.washColor);
        if (left) {
            graphics->fillRect(-originX(), 0, originX(), scene.washHeight);
        } else {
            graphics->fillRect(kGameSpan, 0, context->screen_->width - originX() - kGameSpan,
                               scene.washHeight);
        }
    } else if (!scene.blind && scene.floor != nullptr) {
        if (left) {
            for (int32_t x = -kFloorTile; x >= -originX() - kFloorTile; x -= kFloorTile) {
                graphics->drawImage(scene.floor, x, 0, render::LEFT | render::TOP);
            }
        } else {
            int32_t limit = context->screen_->width - originX() + kFloorTile;
            for (int32_t x = kGameSpan; x <= limit; x += kFloorTile) {
                graphics->drawImage(scene.floor, x, 0, render::LEFT | render::TOP);
            }
        }
    }

    bool authored = false;
    int32_t depth = scene.nearestSideWall != nullptr
                     ? scene.nearestSideWall(scene.ctx, left, &authored)
                     : -1;
    if (depth < 0) {
        return;
    }

    if (!authored) {
        render::Surface *wall = render::surfaceFor(scene.wallAtlas);
        measureRamps(context, wall, scene);
        double scale = 1.0 / (double)(depth + 1);
        int32_t from = left ? -originX() : kGameSpan;
        int32_t to = left ? 0 : context->screen_->width - originX();
        bool filled = true;
        for (int32_t x = from; x < to && filled; ++x) {
            filled = fillPerspectiveColumn(context, context->screen_, wall, art, left, x, scale);
        }
        if (filled) {
            return;
        }
    }

    if (scene.drawSlice == nullptr) {
        return;
    }
    int32_t columns = extraColumns();
    for (int32_t step = 1; step <= columns; ++step) {
        int32_t x = left ? -kColumn * step : kGameSpan + kColumn * (step - 1);
        scene.drawSlice(scene.ctx, graphics, depth, left, x);
    }
}

void applyEdgeFalloff(Context *context, bool left) {
    const double kMaxFalloff = 0.45;
    int32_t bandLeft = originX();
    int32_t bandRight = originX() + kGameSpan;
    int32_t from = left ? 0 : bandRight;
    int32_t to = left ? bandLeft : context->screen_->width;
    if (to <= from) {
        return;
    }
    double span = (double)(to - from);
    for (int32_t x = from; x < to; ++x) {
        double t = left ? (double)(bandLeft - x) / span : (double)(x - bandRight + 1) / span;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double factor = 1.0 - kMaxFalloff * t;
        for (int32_t y = 0; y < context->screen_->height; ++y) {
            uint32_t *pixel = context->screen_->row(y) + x;
            uint32_t argb = *pixel;
            uint32_t r = (uint32_t)(((argb >> 16) & 0xFF) * factor);
            uint32_t g = (uint32_t)(((argb >> 8) & 0xFF) * factor);
            uint32_t b = (uint32_t)((argb & 0xFF) * factor);
            *pixel = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

void extendRows(Context *context, int32_t leftEdge, int32_t rightEdge) {
    for (int32_t y = 0; y < context->screen_->height; ++y) {
        uint32_t *row = context->screen_->row(y);
        uint32_t left = 0xFF000000u | (row[leftEdge] & 0x00FFFFFFu);
        for (int32_t x = 0; x < leftEdge; ++x) {
            row[x] = left;
        }
        uint32_t right = 0xFF000000u | (row[rightEdge] & 0x00FFFFFFu);
        for (int32_t x = rightEdge + 1; x < context->screen_->width; ++x) {
            row[x] = right;
        }
    }
}

}

void Context::postPaintHook(void *context) {
    static_cast<Context *>(context)->postPaint();
}

void Context::postPaint() {
    if (!enabled_ || screen_ == nullptr || provider_ == nullptr) {
        return;
    }
    render::SoftGraphics *graphics = renderer_->screen();
    if (graphics == nullptr) {
        return;
    }
    Scene scene;
    if (!provider_(providerContext_, &scene)) {
        graphics->resetClip();
        extendRows(this, originX(), originX() + kNarrowWidth - 1);
        return;
    }
    if (sceneOwnedExternally_ && scene.dungeonVisible) {
        return;
    }

    int32_t rightEdge = scene.dungeonVisible ? kGameSpan : kNarrowWidth;
    graphics->resetClip();

    if (!scene.dungeonVisible) {
        extendRows(this, originX(), originX() + rightEdge - 1);
        return;
    }

    graphics->setColor(0);
    graphics->fillRect(-originX(), 0, originX(), kHeight);
    graphics->fillRect(rightEdge, 0, screen_->width - originX() - rightEdge, kHeight);

    drawSide(this, graphics, scene, true);
    drawSide(this, graphics, scene, false);
    applyEdgeFalloff(this, true);
    applyEdgeFalloff(this, false);
    graphics->resetClip();
}

int32_t originX() { return (kWideWidth - kNarrowWidth) / 2; }

Context::Context(render::Context *renderer, render::Surface *screen, SceneProvider provider,
                 void *providerContext)
    : renderer_(renderer), screen_(screen), provider_(provider),
      providerContext_(providerContext) {
    if (renderer->screen() != nullptr) {
        renderer->screen()->setOrigin(originX(), 0);
    }
    renderer->setPostPaint(&Context::postPaintHook, this);
}

}
