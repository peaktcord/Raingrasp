#include "src/stormhold/variant.hpp"

#include "src/stormhold/npc_script.hpp"

#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/npc_mechanics.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/smallhelpers.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/vitals.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/extension.hpp"

namespace stormhold {

bool Variant::handleCommand(Command *command) {
    UIWidget *currentUI_ = game_.currentUI_;
    switch (currentUI_->screenId_) {
        case uistate::SCREEN_NPC_GREETING:
        case uistate::SCREEN_NPC_GREETING_ALTERNATE:
        {
            if (command == UIWidget::cmdOk_) {
                if (currentUI_->nextTarget_ == nullptr) {
                    platform::writeLogLine("ERROR: NPC greeting has no follow-on screen; the dialogue will dead-end");
                }
                game_.setCurrentDisplay(currentUI_->nextTarget_);
            }
            return true;
        }
        case screens::NPC_CHOICES_0:
        case screens::NPC_CHOICES_1:
        case screens::NPC_CHOICES_2:
        case screens::NPC_CHOICES_3:
        case screens::NPC_CHOICES_4:
        case screens::NPC_CHOICES_5:
        {
            if (command == UIWidget::cmdCancel_) {
                game_.setCurrentDisplay(currentUI_->backTarget_);
            } else {
                this->handleNPCChoices(currentUI_);
            }
            return true;
        }
        case uistate::SCREEN_NPC_TRAIN_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n4 = currentUI_->contextIndex_;
                int32_t n5 = currentUI_->selectedIndex();
                int32_t n6 = NpcSystem::taughtSkill(n4, n5);
                npcTakeResponseUI_ = this->newNpcResponseUI(currentUI_, n4, uistate::SCREEN_NPC_TRAIN_RESPONSE, 5, n6);
                game_.setCurrentDisplay(npcTakeResponseUI_);
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(currentUI_->contextIndex_);
            }
            return true;
        }
        case uistate::SCREEN_NPC_GIVE_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n8 = currentUI_->contextIndex_;
                int32_t n9 = currentUI_->selectedIndex();
                if (n9 >= 0) {
                    game_.GenericInfoUI_ = this->newNpcResponseUI(currentUI_, n8, uistate::SCREEN_NPC_GIVE_RESPONSE, 4, n9);
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                }
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(currentUI_->contextIndex_);
            }
            return true;
        }
        case screens::NPC_ENCHANT_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n11 = currentUI_->contextIndex_;
                int32_t n12 = currentUI_->selectedIndex() + 87;
                npcTrainResponseUI_ = this->newNpcResponseUI(currentUI_, n11, screens::NPC_ENCHANT_RESPONSE, 7, n12);
                game_.setCurrentDisplay(npcTrainResponseUI_);
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(currentUI_->contextIndex_);
            }
            return true;
        }
        case screens::NPC_TAKE_WHAT:
        {
            if (command == UIWidget::cmdSelect_) {
                int32_t n14 = currentUI_->contextIndex_;
                int32_t n15 = currentUI_->selectedIndex();
                if (n15 >= 0) {
                    npcGiveResponseUI_ = this->newNpcResponseUI(currentUI_, n14, screens::NPC_TAKE_RESPONSE, 8, n15);
                    game_.setCurrentDisplay(npcGiveResponseUI_);
                }
            } else if (command == UIWidget::cmdCancel_) {
                game_.returnToNpcChoices(currentUI_->contextIndex_);
            }
            return true;
        }
        case screens::RETURN_TO_GAME_PAUSED:
        {
            game_.setCurrentDisplay(game_.gameCanvas_);
            game_.gameCanvas_->resume();
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
                NPCTrainWhatUI_ = this->newTrainWhat(n2);
                game_.setCurrentDisplay(NPCTrainWhatUI_);
                break;
            }
            if (n1 == 1) {
                if (game_.character_->itemCount_ <= 0) {
                    game_.GenericInfoUI_->setTitle(NpcSystem::npcNames_[n2]);
                    game_.GenericInfoUI_->setBodyText(std::string("You have nothing to give me!"));
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                    break;
                }
                NPCGiveWhatUI_ = this->newGiveWhat(n2);
                game_.setCurrentDisplay(NPCGiveWhatUI_);
                break;
            }
            if (n1 == 2) {
                npcBefriendUI_ = this->newNpcResponseUI(choicesScreen, n2, uistate::SCREEN_NPC_BEFRIEND_RESPONSE, 2, 0);
                game_.setCurrentDisplay(npcBefriendUI_);
                break;
            }
            if (n1 == 3) {
                npcThreatenUI_ = this->newNpcResponseUI(choicesScreen, n2, uistate::SCREEN_NPC_THREATEN_RESPONSE, 3, 0);
                game_.setCurrentDisplay(npcThreatenUI_);
                break;
            }
            if (n1 != 4) break;
            npcKillUI_ = this->newNpcResponseUI(choicesScreen, n2, screens::NPC_KILL_RESPONSE, 6, 0);
            game_.setCurrentDisplay(npcKillUI_);
            break;
        }
        case 4: {
            if (n1 == 0) {
                if (game_.character_->itemCount_ <= 0) {
                    game_.GenericInfoUI_->setTitle(NpcSystem::npcNames_[n2]);
                    game_.GenericInfoUI_->setBodyText(std::string("You have nothing to give me!"));
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                    break;
                }
                NPCGiveWhatUI_ = this->newGiveWhat(n2);
                game_.setCurrentDisplay(NPCGiveWhatUI_);
                break;
            }
            if (n1 != 1) break;
            NPCTakeWhatUI_ = this->newTakeWhatUI(n2);
            game_.setCurrentDisplay(NPCTakeWhatUI_);
            break;
        }
        case 5: {
            if (n1 == 0) {
                this->showHelgaRumor();
                break;
            }
            if (n1 == 1) {
                if (game_.character_->itemCount_ <= 0) {
                    game_.GenericInfoUI_->setTitle(NpcSystem::npcNames_[n2]);
                    game_.GenericInfoUI_->setBodyText(std::string("You have nothing to give me!"));
                    game_.setCurrentDisplay(game_.GenericInfoUI_);
                    break;
                }
                NPCGiveWhatUI_ = this->newGiveWhat(n2);
                game_.setCurrentDisplay(NPCGiveWhatUI_);
                break;
            }
            if (n1 == 2) {
                NPCEnchantWhatUI_ = this->newEnchantWhatUI(n2);
                game_.setCurrentDisplay(NPCEnchantWhatUI_);
                break;
            }
            if (n1 == 3) {
                npcBlessUI_ = this->newNpcResponseUI(choicesScreen, n2, screens::NPC_BLESS_RESPONSE, 9, 0);
                game_.setCurrentDisplay(npcBlessUI_);
                break;
            }
            if (n1 == 4) {
                npcCureUI_ = this->newNpcResponseUI(choicesScreen, n2, uistate::SCREEN_NPC_CURE_RESPONSE, 10, 0);
                game_.setCurrentDisplay(npcCureUI_);
                break;
            }
            if (n1 == 5) {
                npcWarpUI_ = this->newNpcResponseUI(choicesScreen, n2, uistate::SCREEN_WARP_RESPONSE, 11, 0);
                game_.setCurrentDisplay(npcWarpUI_);
                break;
            }
            if (n1 != 6) break;
            npcRecoveryUI_ = this->newNpcResponseUI(choicesScreen, n2, uistate::SCREEN_NPC_RECOVERY_RESPONSE, 12, 0);
            game_.setCurrentDisplay(npcRecoveryUI_);
        }
    }
}

UIWidget *Variant::newNpcResponseUI(UIWidget *h2, int32_t n, int32_t n2, int32_t n3, int32_t n4) {
    (void)h2;
    UIWidget *h3 = game_.makeOwnedUIWidget(4, n2);
    h3->setupTextBox(std::string("NPC name here"), std::string("NPC text here"));
    std::string string1 = NpcSystem::interact(game_.character_, n, n3, n4).value_or("");
    h3->setTitle(NpcSystem::npcNames_[n]);
    h3->setBodyText(string1);
    h3->contextIndex_ = n;
    return h3;
}

UIWidget *Variant::newGiveWhat(int32_t n) {
    UIWidget *h2 = game_.makeOwnedUIWidget(5, uistate::SCREEN_NPC_GIVE_WHAT);
    h2->contextIndex_ = n;
    SharedArray<std::string> stringArray1(game_.character_->itemCount_);
    int32_t n1 = 0;
    while (n1 < game_.character_->itemCount_) {
        int32_t n2 = wrappingAbs(game_.character_->inventory_[n1]);
        stringArray1[n1] = game_.character_->isEquipped(n1) ? std::string("E:") + Items::nameOf(n2) : Items::nameOf(n2);
        ++n1;
    }
    h2->setupForm(NpcSystem::npcNames_[n], std::string("Give What?"), stringArray1);
    h2->backTarget_ = game_.gameCanvas_;
    return h2;
}

UIWidget *Variant::newTrainWhat(int32_t n) {
    UIWidget *h2 = game_.makeOwnedUIWidget(5, uistate::SCREEN_NPC_TRAIN_WHAT);
    h2->contextIndex_ = n;
    SharedArray<std::string> stringArray1(3);
    int32_t n1 = 0;
    int32_t n2 = 0;
    while (n2 < 14) {
        int32_t n3 = game_.character_->skillRank(n2, false);
        std::string string1 = Player::skillNames_[n2] + " (<TAG>)";
        if (NpcSystem::teaches(n, n2)) {
            stringArray1[n1++] = GameUtil::replace(string1, std::string("<TAG>"), n3);
        }
        ++n2;
    }
    h2->setupForm(NpcSystem::npcNames_[n], std::string("Train What?"), stringArray1);
    h2->backTarget_ = game_.gameCanvas_;
    return h2;
}

UIWidget *Variant::newTakeWhatUI(int32_t n) {
    UIWidget *h2 = game_.makeOwnedUIWidget(5, screens::NPC_ENCHANT_WHAT);
    h2->contextIndex_ = n;
    SharedArray<std::string> stringArray1 = Items::weaponCategoryNames();
    h2->setupForm(NpcSystem::npcNames_[n], std::string("Take What?"), stringArray1);
    h2->backTarget_ = nullptr;
    return h2;
}

UIWidget *Variant::newEnchantWhatUI(int32_t n) {
    UIWidget *h2 = game_.makeOwnedUIWidget(5, screens::NPC_TAKE_WHAT);
    h2->contextIndex_ = n;
    SharedArray<std::string> stringArray1(game_.character_->itemCount_);
    int32_t n1 = 0;
    while (n1 < game_.character_->itemCount_) {
        int32_t n2 = wrappingAbs(game_.character_->inventory_[n1]);
        stringArray1[n1] = Items::nameOf(n2);
        ++n1;
    }
    h2->setupForm(NpcSystem::npcNames_[n], std::string("Enchant What?"), stringArray1);
    h2->backTarget_ = game_.gameCanvas_;
    return h2;
}

void Variant::showHelgaRumor() {
    std::string string1 = NpcSystem::interact(game_.character_, 5, 13, 0).value_or("No rumors!");
    rumorsUI_->setTitle(NpcSystem::npcNames_[5]);
    rumorsUI_->setBodyText(string1);
    rumorsUI_->nextTarget_ = game_.NPCChoicesUI_[5];
    rumorsUI_->contextIndex_ = 5;
    UIWidget *h2 = rumorsUI_->nextTarget_.widget();
    std::string string2 = h2->tagTemplate_;
    std::string string3 = h2->bodyText();
    (void)string3;
    int16_t s1 = 0;
    s1 = game_.worldState_.npcs.gemCount;
    std::string string4 = GameUtil::replace(string2, std::string("<TAG>"), (int32_t)s1);
    h2->setBodyText(string4);
    game_.setCurrentDisplay(rumorsUI_);
}

SharedArray<std::string> NpcSystem::npcNames_;
SharedArray<int8_t> NpcSystem::npcKind_;
SharedArray<int8_t> NpcSystem::npcGridX_;
SharedArray<int8_t> NpcSystem::npcGridY_;
SharedArray<SharedArray<std::string>> NpcSystem::dialogue_;
SharedArray<int32_t> NpcSystem::dialogueCounts_;
bool NpcSystem::dialogueLoaded_ = false;

void NpcSystem::loadDialogue(platform::PlatformContext *context) {
    NpcSystem::loadDialogueFrom(context, std::string("/npcstrings.dat"));
}

void NpcSystem::loadDialogueFrom(platform::PlatformContext *context,
                                 const std::string &string) {
    dialogueLoaded_ = npcmechanics::loadDialogue(context, string, dialogueCounts_, dialogue_);
}

bool NpcSystem::wardenDue(const worldstate::NpcState &state, int32_t n) {
    if (state.wardenVisits == 0 && !state.wardenPresent && n >= 13) {
        return true;
    }
    if (state.wardenVisits == 1 && !state.wardenPresent && n >= 26) {
        return true;
    }
    return state.wardenVisits == 2 && !state.wardenPresent && n >= 39;
}

void NpcSystem::wardenArrive(worldstate::NpcState &state,
                             const worldstate::DungeonRegistry &dungeons) {
    platform::writeLogLine("World: the warden has arrived in camp (visit " +
                           std::to_string((int32_t)state.wardenVisits + 1) + ")");
    state.wardenPresent = true;
    state.wardenVisits = (int8_t)(state.wardenVisits + 1);
    int8_t by1 = npcGridX_[6];
    int8_t by2 = npcGridY_[6];
    SharedArray<int8_t> byArray1 = dungeons[0]->tiles_[by1];
    int8_t by3 = by2;
    byArray1[by3] = (int8_t)(byArray1[by3] | 0x20);
}

void NpcSystem::wardenLeave(worldstate::NpcState &state,
                            const worldstate::DungeonRegistry &dungeons) {
    platform::writeLogLine("World: the warden has left camp");
    int8_t by1 = npcGridX_[6];
    int8_t by2 = npcGridY_[6];
    state.wardenPresent = false;
    int8_t by3 = dungeons[1]->tiles_[by1][by2];
    dungeons[0]->tiles_[by1][by2] = GameUtil::clearFlag((int8_t)32, by3);
}

bool NpcSystem::isChampion(int32_t n) {
    return npcKind_[n] == 1;
}

int32_t NpcSystem::npcAt(const worldstate::NpcState &state, int32_t n, int32_t n2) {
    int32_t n1 = 0;
    while (n1 < 7) {
        if (n == npcGridX_[n1] && n2 == npcGridY_[n1] && state.npcPresent[n1]) {
            return n1;
        }
        ++n1;
    }
    return -1;
}

int32_t NpcSystem::giftValueFor(int32_t n, int32_t n2) {
    return npcmechanics::giftValue(n, n2);
}

std::string NpcSystem::tellRumor(Player *j2, int32_t n) {
    return npcmechanics::trainSkill(*j2, n, dialogue_[7]);
}

std::optional<std::string> NpcSystem::interact(Player *j2, int32_t n, int32_t n2, int32_t n3) {
    worldstate::NpcState &state = j2->world_->worldState().npcs;
    auto &npcPresent_ = state.npcPresent;
    auto &firstMeeting_ = state.firstMeeting;
    auto &befriendDone_ = state.befriendDone;
    auto &threatenDone_ = state.threatenDone;
    auto &aidPoints_ = state.aidPoints;
    auto &suspicion_ = state.suspicion;
    int16_t &scrapCount_ = state.scrapCount;
    int16_t &gemCount_ = state.gemCount;
    switch (n) {
        case 0:
        case 1:
        case 2:
        case 3: {
            if (n2 == 1) {
                if (firstMeeting_[n]) {
                    firstMeeting_[n] = false;
                    return dialogue_[n][0];
                }
                if (suspicion_[n] > 50) {
                    return dialogue_[n][1];
                }
                if (j2->vitals_[8] > 50) {
                    return dialogue_[n][2];
                }
                int32_t n1 = wrappingAbs(j2->world_->worldState().random->nextInt() % 3);
                return dialogue_[n][3 + n1];
            }
            if (n2 == 2) {
                int32_t n4 = npcmechanics::befriend(*j2, state, n);
                if (n4 < 0) {
                    return dialogue_[n][6];
                }
                return dialogue_[n][7 + n4];
            }
            if (n2 == 3) {
                int32_t n8 = npcmechanics::threaten(*j2, state, n, n3);
                if (n8 < 0) {
                    return dialogue_[n][6];
                }
                return dialogue_[n][11 + n8];
            }
            if (n2 == 4) {
                if (befriendDone_[n] == 2 || threatenDone_[n] == 2) {
                    return dialogue_[n][13];
                }
                int32_t n14 = n3;
                int32_t n15 = wrappingAbs(j2->inventory_[n14]);
                if (Items::stat(1, n15) == 15) {
                    j2->removeItem(n14);
                    int32_t n16 = Items::stat(3, n15);
                    int32_t n17 = n;
                    suspicion_[n17] = (int16_t)(suspicion_[n17] - n16);
                    suspicion_[n] = (int16_t)max32(suspicion_[n], 0);
                    return dialogue_[n][17];
                }
                if (Items::stat(1, n15) == 11) {
                    int32_t n18 = npcmechanics::acceptGift(*j2, state, n, n14);
                    return dialogue_[n][13 + n18];
                }
                return dialogue_[n][13];
            }
            if (n2 == 5) {
                if (aidPoints_[n] == 0) {
                    return dialogue_[n][18];
                }
                if (suspicion_[n] > 50) {
                    return dialogue_[n][1];
                }
                if (j2->vitals_[8] > 50) {
                    return dialogue_[n][2];
                }
                int32_t n20 = n;
                aidPoints_[n20] = (int16_t)(aidPoints_[n20] - 1);
                int32_t n21 = n3;
                return NpcSystem::tellRumor(j2, n21);
            }
            if (n2 == 6) {
                npcPresent_[n] = false;
                Dungeon *i2 = static_cast<Dungeon *>(j2->world_->dungeonAt(1));
                i2->tiles_[NpcSystem::npcGridX_[n]][NpcSystem::npcGridY_[n]] = GameUtil::clearFlag((int8_t)32, i2->tiles_[npcGridX_[n]][npcGridY_[n]]);
                return dialogue_[n][19];
            }
            return std::nullopt;
        }
        case 4: {
            if (n2 == 1) {
                if (firstMeeting_[n]) {
                    firstMeeting_[n] = false;
                    return dialogue_[n][0];
                }
                return std::nullopt;
            }
            if (n2 == 4) {
                int32_t n22 = n3;
                int32_t n23 = wrappingAbs(j2->inventory_[n22]);
                int32_t n24 = Items::stat(1, n23);
                if (n24 == 13 || n24 == 15 || n24 == 17) {
                    return dialogue_[n][1];
                }
                scrapCount_ = (int16_t)(scrapCount_ + 1);
                j2->removeItem(n22);
                return dialogue_[n][2];
            }
            if (n2 == 7) {
                if (scrapCount_ / 3 > 0) {
                    int32_t n25 = n3;
                    int16_t s1 = Items::nextId();
                    bool bl1 = j2->addItem(n25, s1, 0);
                    if (!bl1) {
                        return dialogue_[7][0];
                    }
                    scrapCount_ = (int16_t)(scrapCount_ - 3);
                    return dialogue_[n][3];
                }
                return dialogue_[n][4];
            }
            return std::nullopt;
        }
        case 5: {
            if (n2 == 1) {
                if (firstMeeting_[n]) {
                    platform::writeLogLine("NPC: first meeting with npc " + std::to_string(n));
                    firstMeeting_[n] = false;
                    j2->rumorsHeard_ = 0;
                    if (j2->world_->worldState().npcs.wardenPending) {
                        j2->world_->worldState().npcs.wardenPending = false;
                        return dialogue_[5][21] + "\n" + dialogue_[n][0] + "\n" + dialogue_[n][2];
                    }
                    return dialogue_[n][0] + "\n" + dialogue_[n][2];
                }
                int32_t n26 = j2->world_->gameAdvancementLevel((int32_t)j2->giftPoints_);
                if (n26 > j2->rumorsHeard_) {
                    j2->rumorsHeard_ = (int16_t)(j2->rumorsHeard_ + 1);
                    if (j2->world_->worldState().npcs.wardenPending) {
                        j2->world_->worldState().npcs.wardenPending = false;
                        return dialogue_[5][21] + "\n" + dialogue_[n][2 + j2->rumorsHeard_];
                    }
                    return dialogue_[n][2 + j2->rumorsHeard_];
                }
                if (j2->world_->worldState().npcs.wardenPending) {
                    j2->world_->worldState().npcs.wardenPending = false;
                    return dialogue_[5][21];
                }
                return std::nullopt;
            }
            if (n2 == 13) {
                return dialogue_[n][2 + j2->rumorsHeard_];
            }
            if (n2 == 4) {
                int32_t n27 = n3;
                int32_t n28 = wrappingAbs(j2->inventory_[n27]);
                if (Items::stat(1, n28) == 13) {
                    int32_t n29 = Extension::itemTier(*j2, n27);
                    gemCount_ = n29 > 3 ? (int16_t)(gemCount_ + 5) : (int16_t)(gemCount_ + 3);
                    j2->removeItem(n27);
                    return dialogue_[n][11];
                }
                return dialogue_[n][12];
            }
            if (n2 == 8) {
                if (gemCount_ < 7) {
                    return dialogue_[n][1];
                }
                int32_t n30 = n3;
                int32_t n31 = wrappingAbs(j2->inventory_[n30]);
                if (!Items::isWeaponOrArmour(n31) || Extension::isEnchanted(*j2, n30)) {
                    return dialogue_[n][14];
                }
                gemCount_ = (int16_t)(gemCount_ - 7);
                Extension::enchantItem(*j2, n30);
                return dialogue_[n][13];
            }
            if (n2 == 9) {
                if (gemCount_ < 2) {
                    return dialogue_[n][1];
                }
                if (j2->blessed_) {
                    return dialogue_[n][15];
                }
                j2->blessed_ = true;
                gemCount_ = (int16_t)(gemCount_ - 2);
                return dialogue_[n][16];
            }
            if (n2 == 10) {
                if (gemCount_ < 1) {
                    return dialogue_[n][1];
                }
                j2->ailments_ = 0;
                gemCount_ = (int16_t)(gemCount_ - 1);
                return dialogue_[n][17];
            }
            if (n2 == 11) {
                if (gemCount_ < 1) {
                    return dialogue_[n][1];
                }
                if (!j2->hasRecallPoint()) {
                    return dialogue_[n][18];
                }
                gemCount_ = (int16_t)(gemCount_ - 1);
                j2->recall();
                return dialogue_[n][19];
            }
            if (n2 == 12) {
                npcmechanics::recover(*j2);
                return dialogue_[n][20];
            }
            return std::nullopt;
        }
        case 6: {
            if (j2->world_->worldState().npcs.wardenVisits == 0) {
                return std::nullopt;
            }
            if (j2->world_->worldState().npcs.wardenVisits == 1 && ext(j2).wardenStage_ == 0) {
                ext(j2).wardenStage_ = 1;
                return dialogue_[n][0];
            }
            if (j2->world_->worldState().npcs.wardenVisits == 2 && ext(j2).wardenStage_ <= 1) {
                ext(j2).wardenStage_ = (int16_t)2;
                return dialogue_[n][1];
            }
            if (j2->world_->worldState().npcs.wardenVisits == 3 && ext(j2).wardenStage_ <= 2) {
                ext(j2).wardenStage_ = (int16_t)3;
                return dialogue_[n][2];
            }
            if (j2->world_->worldState().npcs.wardenVisits == 4 && ext(j2).wardenStage_ <= 3) {
                ext(j2).wardenStage_ = (int16_t)4;
                return dialogue_[n][3] + "\n" + dialogue_[n][4];
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

bool NpcSystem::isWardenAdjacent(Player *j2) {
    int32_t n1;
    if (j2->dungeonId_ != 1) {
        return false;
    }
    int32_t n2 = wrappingAbs(j2->gridX_ - npcGridX_[6]);
    return n2 + (n1 = wrappingAbs(j2->gridY_ - npcGridY_[6])) == 1;
}

bool NpcSystem::teaches(int32_t n, int32_t n2) {
    switch (n) {
        case 0: {
            switch (n2) {
                case 3:
                case 4:
                case 13: {
                    return true;
                }
            }
            return false;
        }
        case 1: {
            switch (n2) {
                case 7:
                case 8:
                case 10: {
                    return true;
                }
            }
            return false;
        }
        case 2: {
            switch (n2) {
                case 1:
                case 6:
                case 12: {
                    return true;
                }
            }
            return false;
        }
        case 3: {
            switch (n2) {
                case 0:
                case 2:
                case 5: {
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
        case 0: {
            switch (n2) {
                case 0: {
                    return 3;
                }
                case 1: {
                    return 4;
                }
                case 2: {
                    return 13;
                }
            }
            return -1;
        }
        case 1: {
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
        case 2: {
            switch (n2) {
                case 0: {
                    return 1;
                }
                case 1: {
                    return 6;
                }
                case 2: {
                    return 12;
                }
            }
            return -1;
        }
        case 3: {
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
    }
    return -1;
}

void NpcSystem::initializeStatics() {
    npcNames_ = SharedArray<std::string>{std::string("Arantamo"),      std::string("Celegil"), std::string("Favela Dralor"),
                        std::string("Vander"),        std::string("Beneca"),  std::string("Helga"),
                        std::string("Varus")};
    npcKind_ = SharedArray<int8_t>{1, 1, 1, 1, 2, 2, 3};
    npcGridX_ = SharedArray<int8_t>{12, 3, 15, 6, 7, 12, 9};
    npcGridY_ = SharedArray<int8_t>{3, 7, 7, 13, 2, 13, 9};
    dialogueCounts_ = SharedArray<int32_t>{20, 20, 20, 20, 5, 22, 5, 41};
    dialogueLoaded_ = false;
}

}
