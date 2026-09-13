#include "src/common/game/commandflow.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"

bool Game::handleAbilitiesCommand(Command *command) {
    switch (currentUI_->screenId_) {
        case uistate::SCREEN_SKILLS_LIST:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n38 = currentUI_->selectedIndex();
                int32_t n39 = this->character_->skillAt(n38);
                std::string string5 = this->character_->describeSkill(n39);
                this->setCurrentDisplay(variant_->infoBox(uistate::SCREEN_SKILL_INFO,
                                                          std::string("Skill Info"), string5,
                                                          this->SkillsListUI_));
            }
            return true;
        }
        case uistate::SCREEN_SPELLS_LIST:
        {
            int32_t n40;
            if (command == UIWidget::cmdSelect_ && (n40 = currentUI_->selectedIndex()) >= 0) {
                this->SpellInfoUI_ = this->newSpellInfoUI(n40);
                this->currentSpellIndex_ = n40;
                this->setCurrentDisplay(this->SpellInfoUI_);
            }
            return true;
        }
        case uistate::SCREEN_SPELL_INFO:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n41 = this->character_->spellAt(this->currentSpellIndex_);
                this->character_->readiedSpell_ = (int8_t)(n41 + 1);
                const game::DisplayTarget spellsBack = this->SpellsListUI_->backTarget_;
                this->SpellsListUI_ = this->newSpellsListUI();
                this->SpellsListUI_->backTarget_ = spellsBack;
                if (profile_->rebuiltListsRestoreSelection) {
                    this->SpellsListUI_->setSelectedIndex(this->currentSpellIndex_);
                }
                this->setCurrentDisplay(this->SpellsListUI_);
                this->currentSpellIndex_ = -1;
            }
            return true;
        }
        case uistate::SCREEN_LEVEL_UP:
        {
            if (command == UIWidget::cmdSelect_) {
                attribIncr_[currentUI_->contextIndex_] =
                    commandflow::attributeChoice(currentUI_->selectedText(), Player::attributeNames_);
                if (currentUI_->contextIndex_ < 2) {
                    int32_t n45 = currentUI_->contextIndex_ + 1;
                    this->LevelUpUI_ = this->newLevelUpUI(n45 + 1);
                    this->setCurrentDisplay(this->LevelUpUI_);
                } else {
                    commandflow::applyAttributeChoices(this->character_->attributes_, attribIncr_);
                    this->character_->recalcMaxVitals();
                    this->character_->spendLevelUp();
                    platformContext_->playSound(platform::Sound::LevelUp);
                    this->setCurrentDisplay(this->gameCanvas_);
                    this->gameCanvas_->resume();
                }
            }
            return true;
        }
        case uistate::SCREEN_LEVEL_UP_DONE:
        {
            this->setCurrentDisplay(this->gameCanvas_);
            this->gameCanvas_->resume();
            return true;
        }
        default: return false;
    }
}
