#include "src/dawnstar/render/widescreen_scene.hpp"

#include "src/common/game/util.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/common/game/player.hpp"

using namespace dawnstar;

namespace dawnstar_widescreen {

struct Context::Impl {
    Game **gameSlot = nullptr;
    Player *player = nullptr;
    Dungeon *dungeon = nullptr;
    int32_t dungeonId = 0;
    bool gate = false;
};

namespace {

int8_t tileAt(Player *player, Dungeon *dungeon, int32_t dx, int32_t dy) {
    int32_t px = player->gridX_;
    int32_t py = player->gridY_;
    int32_t facing = player->facing_;
    if (facing == 1 || facing == 3) {
        int32_t s1 = facing == 1 ? 1 : -1;
        return dungeon->tileAt(px + dx * s1, py - dy * s1);
    }
    int32_t s2 = facing == 2 ? 1 : -1;
    return dungeon->tileAt(px + dy * s2, py + dx * s2);
}

int32_t nearestSideWall(void *ctx, bool left, bool *authored) {
    Context::Impl *context = (Context::Impl *)ctx;
    int32_t side = left ? -1 : 1;
    context->gate = false;
    *authored = false;
    for (int32_t depth = 0; depth < 4; ++depth) {
        int32_t lateral = (1 + depth) * side;
        int8_t beside = tileAt(context->player, context->dungeon, lateral, 0);
        int8_t ahead = tileAt(context->player, context->dungeon, lateral, 1);
        if (GameUtil::hasFlag((int8_t)1, beside) || GameUtil::hasFlag((int8_t)1, ahead)) {
            return depth;
        }
        if (GameUtil::hasFlag((int8_t)64, beside) || GameUtil::hasFlag((int8_t)64, ahead)) {
            context->gate = true;
            *authored = true;
            return depth;
        }
    }
    return -1;
}

int32_t sliceIndex(int32_t wallType, int32_t depthRow, int32_t side) {
    if (wallType == 12) {
        return depthRow;
    }
    if (side == -1) {
        return 8 + depthRow;
    }
    return 7 - depthRow;
}

void drawSlice(void *ctx, render::SoftGraphics *graphics, int32_t depth, bool left, int32_t x) {
    Context::Impl *context = (Context::Impl *)ctx;
    int32_t slice = sliceIndex(11, depth, left ? -1 : 1);
    graphics->setClip(x, 0, 18, widescreen::kHeight);
    int32_t atlasX = x - Extension::kOracleOffsets[slice];
    if (context->gate) {
        graphics->drawImage(GameCanvas::gateImage_, atlasX, 8, render::LEFT | render::TOP);
    } else if (context->dungeonId != 1) {
        graphics->drawImage(GameCanvas::wallInnerImage_, atlasX, 0, render::LEFT | render::TOP);
    } else {
        graphics->drawImage(GameCanvas::wallRightImage_, atlasX, 0, render::LEFT | render::TOP);
    }
}

bool provideScene(void *opaque, widescreen::Scene *scene) {
    Context::Impl *context = static_cast<Context::Impl *>(opaque);
    if (context->gameSlot == nullptr) return false;
    Game *game = *context->gameSlot;
    if (game == nullptr || game->gameCanvas_ == nullptr) return false;

    GameCanvas *canvas = game->gameCanvas_;
    Player *player = canvas->player_;
    scene->dungeonVisible = game->currentUI_ == nullptr && canvas->screenState_ != 3 && canvas->campState_ != 1 &&
                            canvas->campState_ != 2 && canvas->campState_ != 3 && player != nullptr;
    if (!scene->dungeonVisible) return true;

    Dungeon *dungeon = dungeonOf(player);
    if (dungeon == nullptr) {
        scene->dungeonVisible = false;
        return true;
    }

    context->player = player;
    context->dungeon = dungeon;
    context->dungeonId = dungeon->id_;
    context->gate = false;

    scene->blind = player->hasAilment(3);
    scene->wash = player->hasAilment(4);
    scene->washColor = 0xA00000;
    scene->washHeight = GameCanvas::floorImage_ != nullptr ? GameCanvas::floorImage_->getHeight() : widescreen::kHeight;
    scene->floor = context->dungeonId != 1 ? GameCanvas::floorIceImage_ : GameCanvas::floorImage_;
    scene->wallAtlas = context->dungeonId != 1 ? GameCanvas::wallInnerImage_ : GameCanvas::wallRightImage_;

    scene->left.slope = 1.0;
    scene->left.offset = -158.0;
    scene->left.fitAtlasA = 176;
    scene->left.fitAtlasB = 212;
    scene->left.tileBase = 158;
    scene->left.tileDir = 1;

    scene->right.slope = 1.0;
    scene->right.offset = 18.0;
    scene->right.fitAtlasA = 108;
    scene->right.fitAtlasB = 144;
    scene->right.tileBase = 161;
    scene->right.tileDir = -1;

    scene->nearestSideWall = &nearestSideWall;
    scene->drawSlice = &drawSlice;
    scene->ctx = context;
    return true;
}

}

Context::Context(render::Context *renderer, render::Surface *screen, Game **gameSlot)
    : impl_(std::make_unique<Impl>()) {
    impl_->gameSlot = gameSlot;
    widescreen_ = std::make_unique<widescreen::Context>(renderer, screen, &provideScene,
                                                        impl_.get());
}

Context::~Context() = default;
void Context::setEnabled(bool on) { widescreen_->setEnabled(on); }
bool Context::enabled() const { return widescreen_->enabled(); }
void Context::setSceneOwnedExternally(bool owned) {
    widescreen_->setSceneOwnedExternally(owned);
}

}
