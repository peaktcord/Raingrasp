#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/portoptions_menu.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"

bool Game::handleMenuCommand(Command *command) {
    if (currentUI_->screenId_ == uistate::SCREEN_PORT_OPTIONS) {
        if (command == UIWidget::cmdSelect_) {
            portoptions::select(currentUI_->selectedIndex(), &platformContext_->portOptions());
            platformContext_->notifyPortOptionsChanged();
            currentUI_->setRowLabels(portoptions::labels(platformContext_->portOptions()));
        } else if (command == UIWidget::cmdBack_) {
            this->setCurrentDisplay(currentUI_->backTarget_);
        }
        return true;
    }

    const bool fromOptions = currentUI_->screenId_ == uistate::SCREEN_OPTIONS;
    if (!fromOptions && currentUI_->screenId_ != uistate::SCREEN_MAIN_MENU) return false;
    if (command == UIWidget::cmdSelect_) {
        const auto action =
            fromOptions ? menuaction::at(profile_->optionsRows, profile_->optionsRowCount,
                                         currentUI_->selectedIndex())
                        : menuaction::at(menuaction::kMainMenuRows, menuaction::kMainMenuRowCount,
                                         currentUI_->selectedIndex());
        performMenuAction(action, fromOptions);
    } else if (fromOptions && command == UIWidget::cmdBack_) {
        this->setCurrentDisplay(this->gameCanvas_);
    }
    return true;
}

void Game::performMenuAction(menuaction::Action action, bool fromOptions) {
    switch (action) {
    case menuaction::NEW_GAME: {
        if (profile_->newGameJob == game::NewGameJob::AtMenu) {
            this->createGameUI_ = makeOwnedUIWidget(8, uistate::SCREEN_PROGRESS_NEW_GAME);
            this->createGameUI_->bindCanvasAgain();
            if (profile_->showProgressBeforeJob) {
                this->setCurrentDisplay(this->createGameUI_);
                this->startHelperJob(4);
            } else {
                this->startHelperJob(4);
                this->setCurrentDisplay(this->createGameUI_);
            }
        } else {
            this->setCurrentDisplay(this->newGameUI_);
        }
        return;
    }
    case menuaction::LOAD_GAME: {
        this->gameCanvas_->stopLoop();
        loadGameUI_ = makeOwnedUIWidget(9, uistate::SCREEN_PROGRESS_LOAD_GAME);
        loadGameUI_->bindCanvasAgain();
        if (profile_->loadFailureReturnsToSource) {
            this->noSavedGameUI_->backTarget_ = fromOptions ? this->OptionsUI_ : this->mainMenuUI_;
        }
        if (profile_->showProgressBeforeJob) {
            this->setCurrentDisplay(loadGameUI_);
            this->startHelperJob(6);
        } else {
            this->startHelperJob(6);
            this->setCurrentDisplay(loadGameUI_);
        }
        return;
    }
    case menuaction::PORT_OPTIONS: {
        this->portOptionsUI_ = this->newPortOptionsUI(currentUI_);
        this->setCurrentDisplay(this->portOptionsUI_);
        return;
    }
    case menuaction::HELP: {
        if (profile_->helpListPersists) {
            this->helpUI_->backTarget_ = currentUI_;
        } else {
            this->helpUI_ = this->newHelpUI(currentUI_);
        }
        this->setCurrentDisplay(this->helpUI_);
        return;
    }
    case menuaction::CREDITS: {
        this->setCurrentDisplay(variant_->infoBox(uistate::SCREEN_CREDITS, std::string("Credits"),
                                                  creditsString_, currentUI_));
        return;
    }
    case menuaction::GAME_SELECT: {
        this->requestSwitch();
        return;
    }
    case menuaction::QUIT: {
        this->confirmQuitUI_ = this->newConfirmQuitUI(currentUI_);
        this->setCurrentDisplay(this->confirmQuitUI_);
        return;
    }
    case menuaction::STATS: {
        this->setCurrentDisplay(variant_->infoBox(uistate::SCREEN_STATS, std::string("Stats"),
                                                  this->gameCanvas_->player_->characterSheet()));
        return;
    }
    case menuaction::INVENTORY: {
        this->InventoryUI_ = this->newInventoryUI();
        this->setCurrentDisplay(this->InventoryUI_);
        return;
    }
    case menuaction::SKILLS: {
        this->SkillsListUI_ = this->newSkillsListUI();
        this->setCurrentDisplay(this->SkillsListUI_);
        return;
    }
    case menuaction::SPELLS: {
        this->SpellsListUI_ = this->newSpellsListUI();
        this->setCurrentDisplay(this->SpellsListUI_);
        return;
    }
    case menuaction::SAVE_GAME: {
        saveGameUI_ = makeOwnedUIWidget(10, uistate::SCREEN_PROGRESS_SAVE_GAME);
        saveGameUI_->bindCanvasAgain();
        if (profile_->showProgressBeforeJob) {
            this->setCurrentDisplay(saveGameUI_);
            this->startHelperJob(5);
        } else {
            this->startHelperJob(5);
            this->setCurrentDisplay(saveGameUI_);
        }
        return;
    }
    default:
        variant_->performMenuAction(action);
        return;
    }
}
