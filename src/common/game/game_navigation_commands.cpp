#include "src/common/game/commandflow.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/ui_widget.hpp"

bool Game::handleNavigationCommand(Command *command) {
    using namespace commandflow;
    auto kind = commandflow::Command::Other;
    if (command == UIWidget::cmdCancel_) kind = commandflow::Command::Cancel;
    else if (command == UIWidget::cmdOk_) kind = commandflow::Command::Ok;
    else if (command == UIWidget::cmdSelect_) kind = commandflow::Command::Select;
    else if (command == UIWidget::cmdBack_) kind = commandflow::Command::Back;
    const auto result = navigate(currentUI_->screenId_, kind, currentUI_->backTarget_ != nullptr,
                                 profile_->navigationRules,
                                 (std::size_t)profile_->navigationRuleCount);
    switch (result.destination) {
        case Destination::Back: setCurrentDisplay(currentUI_->backTarget_); break;
        case Destination::Game: setCurrentDisplay(this->gameCanvas_); break;
        case Destination::Next: setCurrentDisplay(currentUI_->nextTarget_); break;
        case Destination::CharacterSheet: setCurrentDisplay(this->characterMainUI_); break;
        case Destination::Help: setCurrentDisplay(this->helpUI_); break;
        case Destination::MainMenu: setCurrentDisplay(this->mainMenuUI_); break;
        case Destination::Options: setCurrentDisplay(this->OptionsUI_); break;
        case Destination::Skills: setCurrentDisplay(this->SkillsListUI_); break;
        case Destination::NpcChoices: returnToNpcChoices(currentUI_->contextIndex_); break;
        default: break;
    }
    return result.recognized;
}

void Game::returnToNpcChoices(int32_t npc) {
    variant_->refreshNpcAid(npc);
    this->setCurrentDisplay(this->NPCChoicesUI_[npc]);
}
