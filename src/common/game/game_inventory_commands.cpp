#include "src/common/game/commandflow.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"

bool Game::handleInventoryCommand(Command *command) {
    switch (currentUI_->screenId_) {
        case uistate::SCREEN_INVENTORY:
        {
            int32_t n20;
            if (command == UIWidget::cmdSelect_ && (n20 = currentUI_->selectedIndex()) >= 0) {
                this->InventoryItemUI_ = this->newInventoryItemUI(n20);
                this->currentItemIndex_ = n20;
                this->setCurrentDisplay(this->InventoryItemUI_);
            }
            return true;
        }
        case uistate::SCREEN_INVENTORY_ITEM:
        {
            if (command == UIWidget::cmdSelect_) {
                const auto action = currentUI_->itemActions_.at(currentUI_->selectedIndex());
                commandflow::executeItemAction(*this->character_, this->currentItemIndex_, action);
                const bool warpCheck = profile_->warpCheckAfterAnyItemAction ||
                                       action == commandflow::ItemAction::Use;
                if (warpCheck && this->character_->warped_) {
                    this->character_->warped_ = false;
                    this->setCurrentDisplay(this->gameCanvas_);
                } else if (action != commandflow::ItemAction::None) {
                    const game::DisplayTarget inventoryBack = this->InventoryUI_->backTarget_;
                    this->InventoryUI_ = this->newInventoryUI();
                    this->InventoryUI_->backTarget_ = inventoryBack;
                    if (profile_->rebuiltListsRestoreSelection) {
                        this->InventoryUI_->setSelectedIndex(this->currentItemIndex_);
                    }
                    this->setCurrentDisplay(this->InventoryUI_);
                }
                this->currentItemIndex_ = -1;
            }
            return true;
        }
        default: return false;
    }
}
