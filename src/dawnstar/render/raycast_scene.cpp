#include "src/dawnstar/render/raycast_scene.hpp"

#include "src/common/game/util.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"

using namespace dawnstar;

namespace dawnstar_raycast {

struct Context::Impl {
    Game **gameSlot = nullptr;
    Dungeon *dungeon = nullptr;
};

namespace {

bool isWall(void *ctx, int32_t mapX, int32_t mapY) {
    Context::Impl *context = static_cast<Context::Impl *>(ctx);
    if (context->dungeon == nullptr) {
        return true;
    }
    int8_t tile = context->dungeon->tileAt(mapX, mapY);
    return GameUtil::hasFlag((int8_t)1, tile);
}

bool provideScene(void *opaque, raycast::Scene *scene) {
    Context::Impl *context = static_cast<Context::Impl *>(opaque);
    if (context->gameSlot == nullptr) return false;
    Game *game = *context->gameSlot;
    if (game == nullptr || game->gameCanvas_ == nullptr) return false;

    GameCanvas *canvas = game->gameCanvas_;
    Player *player = canvas->player_;
    bool dungeonVisible = game->currentUI_ == nullptr && canvas->screenState_ != 3 && canvas->campState_ != 1 &&
                          canvas->campState_ != 2 && canvas->campState_ != 3 && player != nullptr;
    if (!dungeonVisible) {
        return true;
    }
    Dungeon *dungeon = dungeonOf(player);
    if (dungeon == nullptr) {
        return true;
    }
    context->dungeon = dungeon;

    scene->visible = true;
    scene->blind = player->hasAilment(3);
    scene->isWall = &isWall;
    scene->ctx = context;
    scene->playerX = player->gridX_;
    scene->playerY = player->gridY_;
    scene->facing = player->facing_;

    bool camp = dungeon->id_ == 1;
    scene->backdrop = camp ? GameCanvas::floorImage_ : GameCanvas::floorIceImage_;
    scene->flipBackdrop = ((player->gridX_ + player->gridY_) & 1) != 0;
    scene->wall.atlas = camp ? GameCanvas::wallRightImage_ : GameCanvas::wallInnerImage_;
    scene->palette = scene->wall.atlas;

    scene->wall.x = 0;
    scene->wall.y = 8;
    scene->wall.w = 36;
    scene->wall.h = 138;
    scene->wall.repeat = 4.0;
    return true;
}

}

Context::Context(render::Context *renderer, render::Surface *screen, Game **gameSlot)
    : impl_(std::make_unique<Impl>()) {
    impl_->gameSlot = gameSlot;
    raycast_ = std::make_unique<raycast::Context>(renderer, screen, &provideScene,
                                                  impl_.get());
}

Context::~Context() = default;
void Context::setEnabled(bool on) { raycast_->setEnabled(on); }
bool Context::enabled() const { return raycast_->enabled(); }
bool Context::ownsCurrentFrame() const { return raycast_->ownsCurrentFrame(); }

}
