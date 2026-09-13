#ifndef COMMON_GAME_VARIANT_HPP
#define COMMON_GAME_VARIANT_HPP

#include "src/common/game/menuaction.hpp"
#include "src/common/game/ui_host.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"
#include "src/common/render/sprite.hpp"

class GameCanvas;
class UIWidget;

namespace game {

class Variant {
public:
    virtual ~Variant() = default;

    virtual SplashArt loadSplashArt() = 0;

    virtual void loadTables() = 0;

    virtual void loadArt() = 0;

    virtual render::Sprite *loadMonsterSprite(const std::string &name) = 0;

    virtual void allocateScreens() = 0;

    virtual void buildDungeons() = 0;

    virtual void createNewGame() = 0;

    virtual void resumeGame() = 0;
    virtual void openAndRepopulateDungeons(int32_t level) = 0;

    virtual int32_t gameAdvancementLevel(int32_t giftPoints) = 0;

    virtual void afterLoad() {}

    // Names this variant's own screen ids for the log.  The shared ids live in
    // uistate::screenName; the per-variant ones overlap numerically, so only
    // the variant can name them.  Returning nullptr logs the bare number.
    virtual const char *screenName(int32_t screenId) const {
        (void)screenId;
        return nullptr;
    }

    virtual UIWidget *infoWidget(int32_t screenId) = 0;
    virtual UIWidget *infoBox(int32_t screenId, const std::string &title, const std::string &body,
                              DisplayTarget back = nullptr, DisplayTarget next = nullptr) = 0;

    virtual void refreshNpcAid(int32_t npc) = 0;

    virtual std::string introductionText(int32_t page) = 0;

    virtual void showEndOfGame() = 0;
    virtual void showGameOver() = 0;

    virtual bool performMenuAction(menuaction::Action action) {
        (void)action;
        return false;
    }

    virtual bool handleCommand(Command *command) = 0;

    virtual void openNpcScreen(GameCanvas &canvas, int32_t npc) = 0;
};

}

#endif
