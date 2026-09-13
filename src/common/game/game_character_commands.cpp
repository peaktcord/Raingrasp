#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/util.hpp"

bool Game::handleCharacterCommand(Command *command) {
    const int32_t screen = currentUI_->screenId_;
    if (profile_->introScreenCount > 1 && screen == profile_->introSecondScreen) {
        if (command == UIWidget::cmdOk_) {
            this->startPlay();
        }
        return true;
    }
    switch (screen) {
        case uistate::SCREEN_CLASS_SELECT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n2 = currentUI_->selectedIndex();
                std::string string1 = currentUI_->selectedText();
                if (profile_->characterFlow == game::CharacterFlow::PendingAtClassSelect) {
                    this->pendingCharacterStorage_.reset();
                    this->pendingCharacter_ = nullptr;
                    this->pendingCharacterStorage_ = std::make_unique<Player>(this);
                    this->pendingCharacter_ = this->pendingCharacterStorage_.get();
                    this->pendingCharacter_->initFromClass(n2);
                    this->pendingCharacter_->resetForNewLife(n2, false);
                } else {
                    this->characterStorage_.reset();
                    this->character_ = nullptr;
                    this->characterStorage_ = std::make_unique<Player>(this);
                    this->character_ = this->characterStorage_.get();
                    this->character_->initFromClass(n2);
                }
                this->characterMainUI_->setPromptText(1, string1);
                this->setCurrentDisplay(this->characterMainUI_);
            }
            return true;
        }
        case uistate::SCREEN_CHARACTER_SHEET:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n3 = currentUI_->selectedIndex();
                Player *subject =
                    profile_->characterFlow == game::CharacterFlow::PendingAtClassSelect
                        ? this->pendingCharacter_
                        : this->character_;
                if (n3 == 0) {
                    this->setCurrentDisplay(variant_->infoBox(
                        uistate::SCREEN_CLASS_INFO, std::string("Info"), subject->classSummary(),
                        this->characterMainUI_, this->characterMainUI_));
                } else {
                    if (profile_->characterFlow == game::CharacterFlow::PendingAtClassSelect) {
                        this->characterStorage_ = std::move(this->pendingCharacterStorage_);
                        this->character_ = this->characterStorage_.get();
                        this->pendingCharacter_ = this->character_;
                    }
                    this->characterCreatedUI_ = variant_->infoBox(
                        uistate::SCREEN_CHARACTER_CREATED, std::string("New Character"),
                        std::string(profile_->characterCreatedPrompt), this->characterMainUI_);
                    if (profile_->characterCreatedSelects) {
                        this->characterCreatedUI_->b(UIWidget::cmdOk_);
                        this->characterCreatedUI_->a(UIWidget::cmdSelect_);
                        this->characterCreatedUI_->a(UIWidget::cmdCancel_);
                    }
                    this->setCurrentDisplay(this->characterCreatedUI_);
                }
            }
            return true;
        }
        case uistate::SCREEN_CHARACTER_CREATED:
        {
            Command *accept =
                profile_->characterCreatedSelects ? UIWidget::cmdSelect_ : UIWidget::cmdOk_;
            if (command == accept) {
                this->setCurrentDisplay(this->charNameTextForm_);
            }
            return true;
        }
        case uistate::SCREEN_WELCOME:
        {
            this->setCurrentDisplay(variant_->infoBox(uistate::SCREEN_INTRODUCTION,
                                                      std::string("Introduction"),
                                                      variant_->introductionText(0)));
            return true;
        }
        case uistate::SCREEN_INTRODUCTION:
        {
            if (profile_->introScreenCount > 1) {
                this->setCurrentDisplay(variant_->infoBox(profile_->introSecondScreen,
                                                          std::string("Introduction"),
                                                          variant_->introductionText(1)));
            } else if (command == UIWidget::cmdOk_) {
                this->startPlay();
            }
            return true;
        }
        default: return false;
    }
}

void Game::startPlay() {
    this->gameCanvas_->player_ = this->character_;
    if (profile_->characterFlow == game::CharacterFlow::PlaceAtIntroEnd) {
        this->character_->resetForNewLife(this->character_->classId_, false);
    }
    this->gameCanvas_->startLoop();
    this->setCurrentDisplay(this->gameCanvas_);
}

void Game::handleFormCommand(Command *command, Displayable *displayable) {
    if (displayable == this->errorForm_) {
        this->exit();
    } else if (displayable == this->charNameTextForm_) {
        if (command == UIWidget::cmdOk_) {
            TextField *textField = (TextField *)this->charNameTextForm_->get(1);
            std::string string9 = textField->getString();
            if (string9.length() < 3) {
                Alert *alert = new Alert(
                    std::string("Error"),
                    GameUtil::replace(std::string("Your character name must be at least <TAG> letters"),
                                      std::string("<TAG>"), 3));
                alert->setTimeout(-2);
                this->setCurrentDisplay(alert);
            } else {
                this->character_->name_ = string9;
                UIWidget *welcome = variant_->infoBox(
                    uistate::SCREEN_WELCOME, std::string("Welcome"),
                    std::string("Welcome to The Elder Scrolls Travels!"));
                if (profile_->newGameJob == game::NewGameJob::AfterName) {
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
                    this->setCurrentDisplay(welcome);
                }
            }
        } else if (command == UIWidget::cmdCancel_ && profile_->nameFormCancels) {
            this->setCurrentDisplay(this->characterCreatedUI_);
        }
    }
}
