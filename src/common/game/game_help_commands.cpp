#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"

bool Game::handleHelpCommand(Command *command) {
    switch (currentUI_->screenId_) {
        case uistate::SCREEN_CONFIRM_QUIT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n49 = currentUI_->selectedIndex();
                if (n49 == 0) {
                    this->showExitScreen();
                } else {
                    this->setCurrentDisplay(currentUI_->backTarget_);
                }
            }
            return true;
        }
        case uistate::SCREEN_HELP:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n51 = currentUI_->selectedIndex();
                this->setCurrentDisplay(variant_->infoBox(uistate::SCREEN_HELP_TOPIC,
                                                          helpTitles_[n51], helpStrings_[n51],
                                                          this->helpUI_));
            } else {
                this->setCurrentDisplay(currentUI_->backTarget_);
            }
            return true;
        }
        case uistate::SCREEN_NO_SAVED_GAME:
        {
            if (profile_->loadFailureReturnsToSource && currentUI_->backTarget_ == this->OptionsUI_) {
                this->gameCanvas_->startLoop();
            }
            this->setCurrentDisplay(currentUI_->backTarget_);
            return true;
        }
        case uistate::SCREEN_WARP_RESPONSE:
        {
            if (command == UIWidget::cmdOk_) {
                this->character_->warped_ = false;
                this->setCurrentDisplay(this->gameCanvas_);
            }
            return true;
        }
        case uistate::SCREEN_EXIT:
        {
            this->exit();
            return true;
        }
        case uistate::SCREEN_EXIT_ALTERNATE:
        {
            this->exit();
            return true;
        }
        default: return false;
    }
}
