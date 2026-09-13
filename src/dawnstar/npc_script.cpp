#include "src/dawnstar/variant.hpp"

#include "src/dawnstar/npc_script.hpp"

#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/npc_mechanics.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/smallhelpers.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/vitals.hpp"
#include "src/dawnstar/extension.hpp"

namespace dawnstar {

bool Variant::handleCommand(Command *command) {
    UIWidget *uic_ = game_.currentUI_;
    switch (uic_->screenId_) {
        case screens::REGISTRATION_EXIT:
        {
            game_.exit();
            return true;
        }
        case uistate::SCREEN_END_OF_GAME:
        case uistate::SCREEN_GAME_OVER:
        {
            game_.showExitScreen();
            return true;
        }
        case uistate::SCREEN_NPC_GREETING:
        case uistate::SCREEN_NPC_GREETING_ALTERNATE:
        {
            if (command == UIWidget::cmdOk_) {
                game_.setCurrentDisplay(game_.NPCChoicesUI_[uic_->contextIndex_]);
            }
            return true;
        }
        case screens::NPC_CHOICES_0:
        case screens::NPC_CHOICES_1:
        case screens::NPC_CHOICES_2:
        case screens::NPC_CHOICES_3:
        case screens::NPC_CHOICES_4:
        case screens::NPC_CHOICES_5:
        case screens::NPC_CHOICES_6:
        case screens::NPC_CHOICES_7:
        case screens::NPC_CHOICES_8:
        {
            if (command == UIWidget::cmdCancel_) {
                game_.setCurrentDisplay(uic_->backTarget_);
            } else {
                this->handleNPCChoices(uic_);
            }
            return true;
        }
        case uistate::SCREEN_NPC_TRAIN_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n4 = uic_->contextIndex_;
                int32_t n5 = uic_->selectedIndex();
                int32_t n6 = NpcSystem::taughtSkill(n4, n5);
                this->handleNPCAction(n4, uistate::SCREEN_NPC_TRAIN_RESPONSE, 5, n6);
                game_.setCurrentDisplay(game_.GenericInfoUI_);
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(uic_->contextIndex_);
            }
            return true;
        }
        case screens::NPC_SELL_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n8 = uic_->contextIndex_;
                int32_t n9 = uic_->selectedIndex();
                if (n9 >= 0) {
                    game_.currentItemIndex_ = n9;
                    if (game_.character_->isEquipped(n9)) {
                        int32_t n10 = wrappingAbs(game_.character_->inventory_[n9]);
                        SharedArray<std::string> stringArray1{"No", "Yes"};
                        NPCSellSureUI_ = game_.makeOwnedUIWidget(5, screens::NPC_CONFIRM_SALE);
                        NPCSellSureUI_->contextIndex_ = n8;
                        std::string string3 = "You may sell " + Items::nameOf(n10) + " for " +
                                              std::to_string(Items::stat(5, n10)) + ". Confirm?";
                        NPCSellSureUI_->setupForm(NpcSystem::npcNames_[n8], string3, stringArray1);
                        NPCSellSureUI_->b(UIWidget::cmdCancel_);
                        game_.setCurrentDisplay(NPCSellSureUI_);
                    } else {
                        this->handleNPCAction(n8, screens::NPC_SELL_RESPONSE, 15, n9);
                        game_.setCurrentDisplay(game_.GenericInfoUI_);
                    }
                }
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(uic_->contextIndex_);
            }
            return true;
        }
        case screens::NPC_CONFIRM_SALE:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n12 = uic_->selectedIndex();
                if (n12 == 0) {
                    game_.setCurrentDisplay(NPCSellWhatUI_);
                } else {
                    int32_t n13 = uic_->contextIndex_;
                    this->handleNPCAction(n13, screens::NPC_SELL_RESPONSE, 15, game_.currentItemIndex_);
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                }
            }
            return true;
        }
        case screens::NPC_BUY_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n14 = uic_->contextIndex_;
                int32_t n15 = uic_->selectedIndex();
                if (n15 >= 0) {
                    game_.currentItemIndex_ = n15;
                    this->handleNPCAction(n14, screens::NPC_BUY_RESPONSE, 14, n15);
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                }
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(uic_->contextIndex_);
            }
            return true;
        }
        case screens::NPC_QUESTION_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                this->currentQWhat_ = uic_->selectedIndex();
                int32_t n17 = uic_->contextIndex_;
                NPCQuestionWhomUI_ = game_.makeOwnedUIWidget(5, screens::NPC_QUESTION_WHOM);
                NPCQuestionWhomUI_->contextIndex_ = n17;
                SharedArray<std::string> stringArray2(3);
                int32_t n18 = 0;
                int32_t n19 = 0;
                while (n19 < 4) {
                    if (n19 + 5 != n17) {
                        stringArray2[n18] = NpcSystem::npcNames_[n19 + 5];
                        ++n18;
                    }
                    ++n19;
                }
                NPCQuestionWhomUI_->setupForm(NpcSystem::npcNames_[n17], std::string("Ask about whom?"), stringArray2);
                NPCQuestionWhomUI_->backTarget_ = game_.NPCChoicesUI_[n17];
                game_.setCurrentDisplay(NPCQuestionWhomUI_);
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(uic_->contextIndex_);
            }
            return true;
        }
        case screens::NPC_QUESTION_WHOM:
        {
            if (command == UIWidget::cmdSelect_) {
                this->currentQWhom_ = uic_->selectedIndex();
                int32_t n21 = uic_->contextIndex_;
                int32_t n22 = (n21 - 5) * 18 + this->currentQWhat_ * 3 + this->currentQWhom_;
                std::string string4 = "";
                int32_t n23 = this->currentQWhom_;
                if (n23 >= n21 - 5) {
                    ++n23;
                }
                if (!ext(game_.character_).questFlags_[n22]) {
                    ext(game_.character_).questFlags_[n22] = true;
                    int32_t n24 = n21 - 5;
                    game_.worldState_.npcs.aidPoints[n24] =
                        (int16_t)(game_.worldState_.npcs.aidPoints[n24] - 1);
                    n22 = this->currentQWhat_ * 4 + n23;
                    if (n21 - 5 == ext(game_.character_).traitorId_) {
                        if (ext(game_.character_).traitorQuestionCount_ < 3) {
                            ext(game_.character_).traitorQuestionCount_ =
                                (int8_t)(ext(game_.character_).traitorQuestionCount_ + 1);
                        }
                        if (ext(game_.character_).traitorQuestionCount_ == 2 ||
                            (ext(game_.character_).traitorQuestionCount_ == 3 &&
                             this->nextInt(100) < 20)) {
                            string4 = NpcSystem::dialogue_[9][5 + NpcSystem::kClueFalse[n22]];
                            n22 = 72 + this->currentQWhat_ * 3 + this->currentQWhom_;
                            ext(game_.character_).questFlags_[n22] = true;
                        }
                    }
                    if (string4 == std::string("")) {
                        string4 = NpcSystem::dialogue_[9][5 + NpcSystem::kClueTrue[n22]];
                    }
                } else {
                    n22 = this->currentQWhat_ * 4 + n23;
                    string4 = ext(game_.character_).questFlags_[72 + this->currentQWhat_ * 3 + this->currentQWhom_]
                                 ? NpcSystem::dialogue_[9][5 + NpcSystem::kClueFalse[n22]]
                                 : NpcSystem::dialogue_[9][5 + NpcSystem::kClueTrue[n22]];
                }
                game_.GenericInfoUI_->setScreenId(screens::NPC_QUESTION_RESPONSE);
                game_.GenericInfoUI_->setupTextBox(NpcSystem::npcNames_[n21], string4);
                game_.GenericInfoUI_->contextIndex_ = n21;
                game_.setCurrentDisplay(game_.GenericInfoUI_);
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(uic_->contextIndex_);
            }
            return true;
        }
        case uistate::SCREEN_NPC_GIVE_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n26 = uic_->contextIndex_;
                int32_t n27 = uic_->selectedIndex();
                if (n27 >= 0) {
                    this->handleNPCAction(n26, uistate::SCREEN_NPC_GIVE_RESPONSE, 4, n27);
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                }
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(uic_->contextIndex_);
            }
            return true;
        }
        case screens::NPC_TRAVEL:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n29 = uic_->selectedIndex();
                if (n29 == 0) {
                    this->handleNPCAction(4, uistate::SCREEN_WARP_RESPONSE, 11, 0);
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                } else {
                    int32_t n30 = n29 - 1;
                    int32_t n31 = 0;
                    while (n31 < 4) {
                        if (!game_.worldState_.npcs.firstMeeting[5 + n31]) {
                            if (n30 == 0) {
                                n29 = n31;
                                break;
                            }
                            --n30;
                        }
                        ++n31;
                    }
                    if (n29 == 0) {
                        game_.character_->nextDungeon_ = (int8_t)3;
                        game_.character_->dungeonId_ = (int8_t)3;
                    } else if (n29 == 1) {
                        game_.character_->nextDungeon_ = (int8_t)12;
                        game_.character_->dungeonId_ = (int8_t)12;
                    } else if (n29 == 2) {
                        game_.character_->nextDungeon_ = (int8_t)21;
                        game_.character_->dungeonId_ = (int8_t)21;
                    } else if (n29 == 3) {
                        game_.character_->nextDungeon_ = (int8_t)30;
                        game_.character_->dungeonId_ = (int8_t)30;
                    }
                    game_.character_->gridX_ = game_.character_->nextX_ = NpcSystem::npcGridX_[5 + n29];
                    game_.character_->gridY_ = game_.character_->nextY_ = (int8_t)(NpcSystem::npcGridY_[5 + n29] + 1);
                    game_.character_->facing_ = 1;
                    game_.character_->refreshSurroundings();
                    game_.setCurrentDisplay(game_.gameCanvas_);
                }
            }
            return true;
        }
        case screens::NPC_SELL_RESPONSE:
        {
            if (command == UIWidget::cmdOk_) {
                NPCSellWhatUI_ = this->newSellWhat(uic_->contextIndex_);
                NPCSellWhatUI_->setSelectedIndex(game_.currentItemIndex_);
                game_.setCurrentDisplay(NPCSellWhatUI_);
            }
            return true;
        }
        case screens::NPC_BUY_RESPONSE:
        {
            if (command == UIWidget::cmdOk_) {
                NPCBuyWhatUI_ = this->newBuyWhat(uic_->contextIndex_);
                NPCBuyWhatUI_->setSelectedIndex(game_.currentItemIndex_);
                game_.setCurrentDisplay(NPCBuyWhatUI_);
            }
            return true;
        }
        case screens::CAMP_CONFIRM:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n33 = uic_->selectedIndex();
                if (n33 == 0) {
                    game_.character_->warpToCamp(true);
                    game_.character_->warped_ = false;
                    game_.setCurrentDisplay(game_.gameCanvas_);
                } else {
                    game_.setCurrentDisplay(uic_->backTarget_);
                }
            }
            return true;
        }
        case screens::CLUE_LIST:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n37 = uic_->selectedIndex();
                this->newClueLogUI(n37);
                game_.setCurrentDisplay(game_.GenericInfoUI_);
            }
            return true;
        }
        case screens::CLUE_INFO:
        {
            if (command == UIWidget::cmdOk_) {
                game_.setCurrentDisplay(ClueUI_);
            }
            return true;
        }
        case screens::REVEAL_INTRO:
        {
            RevealUI_ = this->newRevealUI();
            game_.setCurrentDisplay(RevealUI_);
            return true;
        }
        case screens::REVEAL_CONFIRM:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n42 = uic_->selectedIndex();
                if (n42 == 0) {
                    RevealUI_ = this->newRevealWhomUI();
                    game_.setCurrentDisplay(RevealUI_);
                } else {
                    game_.setCurrentDisplay(uic_->backTarget_);
                }
            }
            return true;
        }
        case screens::REVEAL_WHOM:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n43 = uic_->selectedIndex();
                std::string text = NpcSystem::dialogue_[9][68] + "\n" + NpcSystem::dialogue_[9][69] + "\n";
                if (n43 == ext(game_.character_).traitorId_) {
                    // A second correct accusation must not hand out a second Star
                    // of Frost; the menu normally short-circuits before here once
                    // revealed, this is the backstop.
                    if (!ext(game_.character_).traitorRevealed_) {
                        ext(game_.character_).giveStarFrost(*game_.character_);
                    }
                    ext(game_.character_).traitorRevealed_ = true;
                    text = text + NpcSystem::dialogue_[9][70];
                } else {
                    text = text + GameUtil::replace(NpcSystem::dialogue_[9][72], std::string("<TAG>"),
                                                    NpcSystem::npcNames_[5 + ext(game_.character_).traitorId_]);
                }
                game_.GenericInfoUI_->setScreenId(screens::REVEAL_RESULT);
                game_.GenericInfoUI_->setupTextBox(std::string("Reveal Traitor"), text);
                game_.character_->placeAtStart(false);
                game_.setCurrentDisplay(game_.GenericInfoUI_);
            }
            return true;
        }
        case screens::REVEAL_RESULT:
        {
            // Start the oracle countdown, but never restart one already running:
            // re-opening Reveal Traitor used to reset it to the beginning.
            if (ext(game_.character_).oracleIndex_ < 0) {
                ext(game_.character_).oracleIndex_ = 1;
            }
            ext(game_.character_).bossKilled_ = true;
            game_.setCurrentDisplay(game_.gameCanvas_);
            return true;
        }
        default: return false;
    }
}

void Variant::handleNPCChoices(UIWidget *choicesScreen) {
    int32_t n1 = choicesScreen->selectedIndex();
    int32_t n2 = choicesScreen->screenId_ - 9;
    switch (n2) {
        case 0:
        case 1:
        case 2:
        case 3: {
            if (n1 == 0) {
                NPCBuyWhatUI_ = this->newBuyWhat(n2);
                game_.setCurrentDisplay(NPCBuyWhatUI_);
                break;
            }
            if (n1 != 1) break;
            if (game_.character_->itemCount_ <= 0) {
                game_.GenericInfoUI_->setScreenId(screens::NPC_SELL_RESPONSE);
                game_.GenericInfoUI_->setupTextBox(NpcSystem::npcNames_[n2], std::string("You have nothing to give me!"));
                game_.GenericInfoUI_->contextIndex_ = n2;
                game_.setCurrentDisplay(game_.GenericInfoUI_);
                break;
            }
            NPCSellWhatUI_ = this->newSellWhat(n2);
            game_.setCurrentDisplay(NPCSellWhatUI_);
            break;
        }
        case 5:
        case 6:
        case 7:
        case 8: {
            if (n1 == 0) {
                NPCTrainWhatUI_ = this->newTrainWhat(n2);
                game_.setCurrentDisplay(NPCTrainWhatUI_);
                break;
            }
            if (n1 == 1) {
                if (game_.character_->itemCount_ <= 0) {
                    game_.GenericInfoUI_->setScreenId(uistate::SCREEN_NPC_GIVE_RESPONSE);
                    game_.GenericInfoUI_->setupTextBox(NpcSystem::npcNames_[n2],
                                                 std::string("You have nothing to give me!"));
                    game_.GenericInfoUI_->contextIndex_ = n2;
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                    break;
                }
                NPCGiveWhatUI_ = this->newGiveWhat(n2);
                game_.setCurrentDisplay(NPCGiveWhatUI_);
                break;
            }
            if (n1 == 2) {
                this->handleNPCAction(n2, uistate::SCREEN_NPC_BEFRIEND_RESPONSE, 2, 0);
                game_.setCurrentDisplay(game_.GenericInfoUI_);
                break;
            }
            if (n1 == 3) {
                this->handleNPCAction(n2, uistate::SCREEN_NPC_THREATEN_RESPONSE, 3, 0);
                game_.setCurrentDisplay(game_.GenericInfoUI_);
                break;
            }
            if (n1 == 4) {
                if (game_.worldState_.npcs.aidPoints[n2 - 5] == 0) {
                    std::string string1 = NpcSystem::dialogue_[n2][15];
                    game_.GenericInfoUI_->setScreenId(screens::NPC_QUESTION_RESPONSE);
                    game_.GenericInfoUI_->setupTextBox(NpcSystem::npcNames_[n2], string1);
                    game_.GenericInfoUI_->contextIndex_ = n2;
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                    break;
                }
                NPCQuestionWhatUI_->contextIndex_ = n2;
                NPCQuestionWhatUI_->setTitle(NpcSystem::npcNames_[n2]);
                NPCQuestionWhatUI_->backTarget_ = game_.NPCChoicesUI_[n2];
                game_.setCurrentDisplay(NPCQuestionWhatUI_);
                break;
            }
            if (n1 != 5) break;
            NPCWarpUI_ = game_.makeOwnedUIWidget(5, screens::CAMP_CONFIRM);
            NPCWarpUI_->contextIndex_ = n2;
            {
                SharedArray<std::string> stringArray1{"Yes", "No"};
                NPCWarpUI_->setupForm(NpcSystem::npcNames_[n2], std::string("Warp to camp"), stringArray1);
            }
            NPCWarpUI_->backTarget_ = game_.NPCChoicesUI_[n2];
            game_.setCurrentDisplay(NPCWarpUI_);
            break;
        }
        case 4: {
            if (n1 == 0) {
                std::string string2 =
                    NpcSystem::interact(game_.character_, 4, 13, 0).value_or("I have no new rumors.");
                game_.GenericInfoUI_->setScreenId(uistate::SCREEN_NPC_GREETING_ALTERNATE);
                game_.GenericInfoUI_->setupTextBox(NpcSystem::npcNames_[4], string2);
                game_.GenericInfoUI_->contextIndex_ = 4;
                game_.setCurrentDisplay(game_.GenericInfoUI_);
                break;
            }
            if (n1 == 1) {
                this->handleNPCAction(n2, uistate::SCREEN_NPC_CURE_RESPONSE, 10, 0);
                game_.setCurrentDisplay(game_.GenericInfoUI_);
                break;
            }
            if (n1 == 2) {
                WarpWhereUI_ = this->newWarpWhere();
                game_.setCurrentDisplay(WarpWhereUI_);
                break;
            }
            if (n1 != 3) break;
            this->handleNPCAction(n2, uistate::SCREEN_NPC_RECOVERY_RESPONSE, 12, 0);
            game_.setCurrentDisplay(game_.GenericInfoUI_);
        }
    }
}

void Variant::handleNPCAction(int32_t n, int32_t n2, int32_t n3, int32_t n4) {
    game_.GenericInfoUI_->setScreenId(n2);
    std::string string1 = NpcSystem::interact(game_.character_, n, n3, n4).value_or("");
    game_.GenericInfoUI_->setupTextBox(NpcSystem::npcNames_[n], string1);
    game_.GenericInfoUI_->contextIndex_ = n;
}

UIWidget *Variant::newGiveWhat(int32_t n) {
    UIWidget *giveWhatScreen = game_.makeOwnedUIWidget(5, uistate::SCREEN_NPC_GIVE_WHAT);
    giveWhatScreen->contextIndex_ = n;
    SharedArray<std::string> stringArray1(game_.character_->itemCount_);
    int32_t n1 = 0;
    while (n1 < game_.character_->itemCount_) {
        int32_t n2 = wrappingAbs(game_.character_->inventory_[n1]);
        stringArray1[n1] =
            game_.character_->isEquipped(n1) ? "E:" + Items::nameOf(n2) : Items::nameOf(n2);
        ++n1;
    }
    giveWhatScreen->setupForm(NpcSystem::npcNames_[n], std::string("Give What?"), stringArray1);
    giveWhatScreen->backTarget_ = game_.NPCChoicesUI_[n];
    return giveWhatScreen;
}

UIWidget *Variant::newSellWhat(int32_t n) {
    UIWidget *sellWhatScreen = game_.makeOwnedUIWidget(5, screens::NPC_SELL_WHAT);
    sellWhatScreen->contextIndex_ = n;
    SharedArray<std::string> stringArray1(game_.character_->itemCount_);
    int32_t n1 = 0;
    while (n1 < game_.character_->itemCount_) {
        int32_t n2 = wrappingAbs(game_.character_->inventory_[n1]);
        std::string price = " (" + std::to_string(Items::stat(5, n2)) + ")";
        stringArray1[n1] = game_.character_->isEquipped(n1)
                              ? "E:" + Items::nameOf(n2) + price
                              : Items::nameOf(n2) + price;
        ++n1;
    }
    sellWhatScreen->setupForm(NpcSystem::npcNames_[n], std::string("Sell What?"), stringArray1);
    sellWhatScreen->backTarget_ = nullptr;
    return sellWhatScreen;
}

UIWidget *Variant::newBuyWhat(int32_t n) {
    UIWidget *buyWhatScreen = game_.makeOwnedUIWidget(5, screens::NPC_BUY_WHAT);
    buyWhatScreen->contextIndex_ = n;
    int32_t n1 = NpcSystem::stockLists_[n].length();
    SharedArray<std::string> stringArray1(n1);
    int32_t n2 = 0;
    while (n2 < n1) {
        int32_t n3 = wrappingAbs(NpcSystem::stockLists_[n][n2]);
        stringArray1[n2] =
            Items::nameOf(n3) + " (" + std::to_string(Items::stat(4, n3)) + ")";
        ++n2;
    }
    buyWhatScreen->setupForm(NpcSystem::npcNames_[n], std::string("Buy What?"), stringArray1);
    return buyWhatScreen;
}

UIWidget *Variant::newTrainWhat(int32_t n) {
    UIWidget *trainWhatScreen = game_.makeOwnedUIWidget(5, uistate::SCREEN_NPC_TRAIN_WHAT);
    trainWhatScreen->contextIndex_ = n;
    SharedArray<std::string> stringArray1(3);
    int32_t n1 = 0;
    int32_t n2 = 0;
    while (n2 < 14) {
        if (NpcSystem::teaches(n, n2)) {
            int32_t n3 = game_.character_->skillRank(n2, false);
            std::string string1 = Player::skillNames_[n2] + " (<TAG>)";
            stringArray1[n1++] = GameUtil::replace(string1, std::string("<TAG>"), n3);
        }
        ++n2;
    }
    trainWhatScreen->setupForm(NpcSystem::npcNames_[n], std::string("Train What?"), stringArray1);
    trainWhatScreen->backTarget_ = game_.NPCChoicesUI_[n];
    return trainWhatScreen;
}

UIWidget *Variant::newWarpWhere() {
    UIWidget *warpWhereScreen = game_.makeOwnedUIWidget(5, screens::NPC_TRAVEL);
    warpWhereScreen->contextIndex_ = 4;
    std::vector<std::string> destinations;
    destinations.push_back(std::string("Your last location"));
    int32_t n1 = 0;
    while (n1 < 4) {
        if (!game_.worldState_.npcs.firstMeeting[5 + n1]) {
            destinations.push_back(NpcSystem::npcNames_[5 + n1]);
        }
        ++n1;
    }
    int32_t n2 = (int32_t)destinations.size();
    SharedArray<std::string> stringArray1(n2);
    int32_t n3 = 0;
    while (n3 < n2) {
        stringArray1[n3] = destinations[(size_t)n3];
        ++n3;
    }
    warpWhereScreen->setupForm(std::string("Eustacia"), std::string(""), stringArray1);
    warpWhereScreen->backTarget_ = game_.NPCChoicesUI_[4];
    return warpWhereScreen;
}

SharedArray<std::string> NpcSystem::npcNames_;
SharedArray<int8_t> NpcSystem::npcKind_;
SharedArray<int8_t> NpcSystem::npcGridX_;
SharedArray<int8_t> NpcSystem::npcGridY_;
SharedArray<SharedArray<std::string>> NpcSystem::dialogue_;
SharedArray<int32_t> NpcSystem::dialogueCounts_;
bool NpcSystem::dialogueLoaded_ = false;
SharedArray<SharedArray<int8_t>> NpcSystem::stockLists_;
const int8_t NpcSystem::kTraitorRumors[4][6] = {{1, 3, 5, 8, 10, 12},
                           {1, 2, 4, 7, 9, 12},
                           {2, 3, 6, 7, 10, 11},
                           {2, 4, 5, 8, 9, 11}};
const int8_t NpcSystem::kClueTrue[24] = {13, 19, 25, 31, 14, 20, 26, 32, 15, 21, 27, 33,
                         17, 23, 29, 35, 16, 22, 28, 34, 18, 24, 30, 36};
const int8_t NpcSystem::kClueFalse[24] = {37, 43, 49, 55, 38, 44, 50, 56, 39, 45, 51, 57,
                         41, 47, 53, 59, 40, 46, 52, 58, 42, 48, 54, 60};

void NpcSystem::initializeStatics() {
    npcNames_ = SharedArray<std::string>{"Weapon Peddler", "Heavy Armor Peddler", "Light Armor Peddler",
                        "Jakar's", "Eustacia", "Alhavara", "Beatrice", "Chung",
                        "Delacroix"};
    npcKind_ = SharedArray<int8_t>{4, 4, 4, 4, 2, 1, 1, 1, 1};
    npcGridX_ = SharedArray<int8_t>{12, 6, 7, 12, 12, 1, 1, 1, 1};
    npcGridY_ = SharedArray<int8_t>{12, 11, 7, 8, 6, 1, 1, 1, 1};
    dialogueCounts_ = SharedArray<int32_t>{3, 3, 3, 3, 14, 16, 16, 16, 16, 77};
    dialogueLoaded_ = false;
    stockLists_ = SharedArray<SharedArray<int8_t>>(4);
    stockLists_[0] = SharedArray<int8_t>{1, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 14, 15, 17, 18, 19, 20};
    stockLists_[1] = SharedArray<int8_t>{22, 23, 24, 25, 32, 33, 34, 35, 47, 48, 49, 50};
    stockLists_[2] = SharedArray<int8_t>{27, 28, 29, 30, 37, 38, 39, 40, 42, 43, 44, 45};
    stockLists_[3] = SharedArray<int8_t>{87, 88, 89, 90, 91, 93, 94, 95, 96};
}

void NpcSystem::loadDialogue(platform::PlatformContext *context) {
    NpcSystem::loadDialogueFrom(context, std::string("/npcstrings.dat"));
}

void NpcSystem::loadDialogueFrom(platform::PlatformContext *context,
                                 const std::string &string) {
    dialogueLoaded_ = npcmechanics::loadDialogue(context, string, dialogueCounts_, dialogue_);
}

bool NpcSystem::isChampion(int32_t n) {
    return npcKind_[n] == 1;
}

bool NpcSystem::isPeddler(int32_t n) {
    return npcKind_[n] == 4;
}

int32_t NpcSystem::npcAt(int32_t n, int32_t n2) {
    int32_t n1 = 0;
    while (n1 < 5) {
        if (n == npcGridX_[n1] && n2 == npcGridY_[n1]) {
            return n1;
        }
        ++n1;
    }
    return -1;
}

int32_t NpcSystem::giftValueFor(int32_t n, int32_t n2) {
    return npcmechanics::giftValue(n - 5, n2);
}

std::string NpcSystem::tellRumor(Player *j2, int32_t n) {
    return npcmechanics::trainSkill(*j2, n, dialogue_[9]);
}

std::optional<std::string> NpcSystem::interact(Player *j2, int32_t n, int32_t n2, int32_t n3) {
    worldstate::NpcState &state = j2->world_->worldState().npcs;
    auto &firstMeeting_ = state.firstMeeting;
    auto &befriendDone_ = state.befriendDone;
    auto &threatenDone_ = state.threatenDone;
    auto &aidPoints_ = state.aidPoints;
    switch (n) {
        case 0:
        case 1:
        case 2:
        case 3: {
            if (n2 == 1) {
                return dialogue_[n][0];
            }
            if (n2 == 14) {
                int32_t n1 = n3;
                int8_t by1 = NpcSystem::stockLists_[n][n1];
                int32_t n4 = Items::stat(4, (int32_t)by1);
                if (n4 > j2->gold_) {
                    return dialogue_[n][1];
                }
                int16_t s1 = Items::nextId();
                bool bl1 = j2->addItem(by1, s1, 0);
                if (bl1) {
                    j2->addGold(-n4);
                    return dialogue_[n][2];
                }
                return "Sorry, but your pack is too full.";
            }
            if (n2 == 15) {
                int32_t n5 = n3;
                int32_t n6 = wrappingAbs(j2->inventory_[n5]);
                if (Items::stat(1, n6) == 11) {
                    return "Sorry, you may not sell a gift item.  It should be given to "
                           "one of the champions.";
                }
                int32_t n7 = Items::stat(5, n6);
                j2->addGold(n7);
                j2->removeItem(n5);
                return "For that you can have " + std::to_string(n7) + " gold.";
            }
            return "quack";
        }
        case 5:
        case 6:
        case 7:
        case 8: {
            if (n2 == 1) {
                if (firstMeeting_[n]) {
                    firstMeeting_[n] = false;
                    return dialogue_[n][0];
                }
                int32_t n8 = wrappingAbs(j2->world_->worldState().random->nextInt() % 3);
                return dialogue_[n][1 + n8];
            }
            if (n2 == 2) {
                int32_t n9 = npcmechanics::befriend(*j2, state, n - 5);
                if (n9 < 0) {
                    return dialogue_[n][4];
                }
                return dialogue_[n][5 + n9];
            }
            if (n2 == 3) {
                int32_t n13 = npcmechanics::threaten(*j2, state, n - 5, n3);
                if (n13 < 0) {
                    return dialogue_[n][4];
                }
                return dialogue_[n][9 + n13];
            }
            if (n2 == 4) {
                if (befriendDone_[n - 5] == 2 || threatenDone_[n - 5] == 2) {
                    return dialogue_[n][11];
                }
                int32_t n18 = n3;
                int32_t n19 = wrappingAbs(j2->inventory_[n18]);
                if (Items::stat(1, n19) == 11) {
                    int32_t n20 = npcmechanics::acceptGift(*j2, state, n - 5, n18);
                    return dialogue_[n][11 + n20];
                }
                return dialogue_[n][11];
            }
            if (n2 == 5) {
                if (aidPoints_[n - 5] == 0) {
                    return dialogue_[n][15];
                }
                int32_t n22 = n - 5;
                aidPoints_[n22] = (int16_t)(aidPoints_[n22] - 1);
                int32_t n23 = n3;
                return NpcSystem::tellRumor(j2, n23);
            }
            if (n2 != 8) {
                return "quack";
            }
        }
        case 4: {
            if (n2 == 1) {
                if (firstMeeting_[n]) {
                    firstMeeting_[n] = false;
                    j2->rumorsHeard_ = 0;
                    if (j2->world_->worldState().npcs.wardenPending) {
                        j2->world_->worldState().npcs.wardenPending = false;
                        return dialogue_[4][13] + "\n \n" + dialogue_[4][0] + "\n \n" + dialogue_[4][1] +
                               "\n \n" + dialogue_[4][2];
                    }
                    return dialogue_[4][0] + "\n \n" + dialogue_[4][1] + "\n \n" + dialogue_[4][2];
                }
                int32_t n24 = j2->world_->gameAdvancementLevel(j2->giftPoints_);
                if (n24 > j2->rumorsHeard_) {
                    j2->rumorsHeard_ = (int16_t)(j2->rumorsHeard_ + 1);
                }
                if (j2->world_->worldState().npcs.wardenPending) {
                    j2->world_->worldState().npcs.wardenPending = false;
                    return dialogue_[4][13];
                }
                return std::nullopt;
            }
            if (n2 == 13) {
                int32_t n25 = 0;
                int32_t n26 = 0;
                while (n26 < 6) {
                    if (ext(j2).questFlags_[90 + n26]) {
                        ++n25;
                    }
                    ++n26;
                }
                std::string string1 = "";
                if (n25 <= j2->rumorsHeard_) {
                    string1 = dialogue_[n][3 + j2->rumorsHeard_];
                    int32_t n27 = 0;
                    if (n25 < 6) {
                        n27 = GameUtil::randomInt(j2->world_->worldState().random, 6 - n25) - 1;
                    }
                    int32_t n28 = 0;
                    while (n28 < 6) {
                        if (!ext(j2).questFlags_[90 + n28]) {
                            if (n27 == 0) {
                                n27 = n28;
                                break;
                            }
                            --n27;
                        }
                        ++n28;
                    }
                    ext(j2).questFlags_[90 + n27] = true;
                    string1 = GameUtil::replace(string1, std::string("<TAG>"), dialogue_[9][5 + kTraitorRumors[ext(j2).traitorId_][n27]]);
                } else {
                    string1 = "I have no new rumors.";
                }
                return string1;
            }
            if (n2 == 10) {
                j2->ailments_ = 0;
                return dialogue_[n][9];
            }
            if (n2 == 11) {
                if (!j2->hasRecallPoint()) {
                    return dialogue_[n][10];
                }
                j2->recall();
                return dialogue_[n][11];
            }
            if (n2 == 12) {
                npcmechanics::recover(*j2);
                return dialogue_[n][12];
            }
            return std::nullopt;
        }
    }
    return "quack2";
}

bool NpcSystem::teaches(int32_t n, int32_t n2) {
    switch (n) {
        case 5: {
            switch (n2) {
                case 7:
                case 8:
                case 10: {
                    return true;
                }
            }
            return false;
        }
        case 6: {
            switch (n2) {
                case 1:
                case 3:
                case 4: {
                    return true;
                }
            }
            return false;
        }
        case 7: {
            switch (n2) {
                case 0:
                case 2:
                case 5: {
                    return true;
                }
            }
            return false;
        }
        case 8: {
            switch (n2) {
                case 6:
                case 12:
                case 13: {
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

int32_t NpcSystem::taughtSkill(int32_t n, int32_t n2) {
    switch (n) {
        case 5: {
            switch (n2) {
                case 0: {
                    return 7;
                }
                case 1: {
                    return 8;
                }
                case 2: {
                    return 10;
                }
            }
            return -1;
        }
        case 6: {
            switch (n2) {
                case 0: {
                    return 1;
                }
                case 1: {
                    return 3;
                }
                case 2: {
                    return 4;
                }
            }
            return -1;
        }
        case 7: {
            switch (n2) {
                case 0: {
                    return 0;
                }
                case 1: {
                    return 2;
                }
                case 2: {
                    return 5;
                }
            }
            return -1;
        }
        case 8: {
            switch (n2) {
                case 0: {
                    return 6;
                }
                case 1: {
                    return 12;
                }
                case 2: {
                    return 13;
                }
            }
            return -1;
        }
    }
    return -1;
}

}
