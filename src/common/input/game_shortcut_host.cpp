#include "src/common/input/game_shortcut_host.hpp"

#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"

namespace game_shortcuts {
namespace {

Game **g_gameSlot = nullptr;
int32_t g_extraRootScreen = -1;
int32_t g_shortcutRootScreen = -1;

Game *game() { return g_gameSlot != nullptr ? *g_gameSlot : nullptr; }

void pressKey(int32_t code) {
    Game *g = game();
    if (code == 55 && g != nullptr && g->gameCanvas_ != nullptr &&
        g->currentUI_ == nullptr) {
        g->gameCanvas_->openOptions();
        return;
    }
    Canvas *canvas = g != nullptr && g->display_ != nullptr
                         ? dynamic_cast<Canvas *>(g->display_->getCurrent())
                         : nullptr;
    if (canvas != nullptr) {
        canvas->keyPressed(code);
        canvas->keyReleased(code);
    }
}

bool optionsOpen() {
    Game *g = game();
    return g != nullptr && g->currentUI_ != nullptr &&
           g->currentUI_->screenId_ == uistate::SCREEN_OPTIONS;
}

bool selectRow(int32_t row) {
    Game *g = game();
    if (g == nullptr || g->currentUI_ == nullptr || row < 0 ||
        row >= g->currentUI_->rowCount()) {
        return false;
    }
    g->currentUI_->setSelectedIndex(row);
    if (!g->handleMenuCommand(UIWidget::cmdSelect_) || g->currentUI_ == nullptr) return false;
    const int32_t screen = g->currentUI_->screenId_;
    const bool isRoot = screen == uistate::SCREEN_STATS ||
                        screen == uistate::SCREEN_INVENTORY ||
                        screen == uistate::SCREEN_SKILLS_LIST ||
                        screen == uistate::SCREEN_SPELLS_LIST ||
                        screen == uistate::SCREEN_HELP || screen == g_extraRootScreen;
    if (!isRoot) return false;
    g->currentUI_->backTarget_ = g->gameCanvas_;
    g_shortcutRootScreen = screen;
    return true;
}

bool inGame() {
    Game *g = game();
    return g != nullptr && g->gameCanvas_ != nullptr && g->currentUI_ == nullptr &&
           g->gameCanvas_->player_ != nullptr;
}

bool atShortcutRoot() {
    Game *g = game();
    return g != nullptr && g->currentUI_ != nullptr &&
           g->currentUI_->screenId_ == g_shortcutRootScreen;
}

bool closeShortcutMenu() {
    Game *g = game();
    if (g == nullptr || g->currentUI_ == nullptr || g->gameCanvas_ == nullptr) return false;
    g->setCurrentDisplay(g->gameCanvas_);
    return true;
}

const shortcuts::Host kHost = {
    &pressKey, &optionsOpen, &selectRow, &inGame, &atShortcutRoot, &closeShortcutMenu};

}

void install(Game **gameSlot, int32_t extraRootScreen) {
    g_gameSlot = gameSlot;
    g_extraRootScreen = extraRootScreen;
    g_shortcutRootScreen = -1;
}

const shortcuts::Host *host() { return &kHost; }

}
