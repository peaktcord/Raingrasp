#include "src/stormhold/extension.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/util.hpp"
#include "src/common/render/minimap.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/variant.hpp"
#include "src/stormhold/npc_script.hpp"

namespace stormhold {

void Extension::onNewLife(Player &player) {
    (void)player;
    this->wardenStage_ = 0;
}

void Extension::beforeMove(Player &player) {
    dungeonOf(player)->refreshMonsterBits((int32_t)player.gridX_, (int32_t)player.gridY_);
}

void Extension::beforeStep(Player &player) {
    worldstate::WorldState &worldState = player.world_->worldState();
    if (player.nextDungeon_ == 37 && player.dungeonId_ != 37) {
        Monster decodedMonster;
        for (const SharedArray<int8_t> &byArray1 :
             worldState.monsters.at((std::size_t)(player.nextDungeon_ - 1))) {
            Monster *monster = Monster::fromRecord(&decodedMonster, byArray1,
                                                   player.world_->dungeonAt(player.nextDungeon_));
            if (monster->type_ != 41) continue;
            monster->hp_ = (int8_t)monster->stat(14);
            monster->store();
        }
    }
    this->enteredSafeZone_ =
        !isSafeZone((int32_t)player.dungeonId_, player.gridX_, player.gridY_) &&
        isSafeZone((int32_t)player.nextDungeon_, player.nextX_, player.nextY_);
    this->leftSafeZone_ =
        isSafeZone((int32_t)player.dungeonId_, player.gridX_, player.gridY_) &&
        !isSafeZone((int32_t)player.nextDungeon_, player.nextX_, player.nextY_);
}

bool Extension::takeItemsUnderfoot(Player &player, int8_t tile, bool forward) {
    (void)tile;
    if (!forward) {
        return false;
    }
    Dungeon *i2 = dungeonOf(player);
    int32_t n1 = i2->countDroppedItems(player.gridX_, player.gridY_);
    if (n1 == 1) {
        SharedArray<int8_t> object1 = i2->firstDroppedItem(player.gridX_, player.gridY_);
        if ((object1[6] & 4) != 0) {
            player.gameWon_ = true;
            return true;
        }
        platform::writeLogLine("Player: picked up dropped item: " + describeRecord(object1));
        bool bl4 = player.takeDroppedItem(object1);
        if (!bl4) return false;
        i2->removeDroppedItem(object1);
        if ((object1[6] & 2) != 0) return false;
        int32_t var1 = object1[2] - 1;
        if (Items::at(var1).category != 11) return false;
        player.giftPoints_ = (int16_t)(player.giftPoints_ + (int16_t)Items::at(var1).tier);
        this->onGiftPoints(player);
        return false;
    }
    if (n1 > 1) {
        worldstate::DroppedItemList items = i2->droppedItemsAt(player.gridX_, player.gridY_);
        for (const SharedArray<int8_t> &byArray2 : items) {
            if ((byArray2[6] & 4) != 0) {
                player.gameWon_ = true;
                return true;
            }
            bool bl5 = player.takeDroppedItem(byArray2);
            if (!bl5) continue;
            i2->removeDroppedItem(byArray2);
            if ((byArray2[6] & 2) == 0) continue;
            int32_t n3 = byArray2[2] - 1;
            if (Items::at(n3).category != 11) continue;
            player.giftPoints_ = (int16_t)(player.giftPoints_ + (int16_t)Items::at(n3).tier);
            this->onGiftPoints(player);
        }
    }
    return false;
}

void Extension::onGiftPoints(Player &player) {
    int32_t level = player.world_->gameAdvancementLevel((int32_t)player.giftPoints_);
    player.world_->openAndRepopulateDungeons(level);
}

void Extension::placeVisibleNpcs(Player &player) {
    if (player.dungeonId_ == 1 && player.world_->worldState().npcs.wardenPresent) {
        player.placeVisible(5, visibility::Slot(std::string("W")));
    }
}

bool Extension::npcPosition(const Player &player, int32_t kind, const visibility::Slot &object, int8_t &x,
                            int8_t &y) {
    (void)player;
    (void)object;
    if (kind != 5) {
        return false;
    }
    x = NpcSystem::npcGridX_[6];
    y = NpcSystem::npcGridY_[6];
    return true;
}

int32_t Extension::npcAhead(const Player &player, int32_t dungeonId, int32_t x, int32_t y) {
    if (dungeonId != 1) {
        return -1;
    }
    return NpcSystem::npcAt(player.world_->worldState().npcs, x, y);
}

std::string Extension::itemEffectText(int32_t index) {
    SharedArray<std::string> stringArray1 = stormhold::itemEffectText[index];
    std::string string1 = stringArray1[0];
    if (stringArray1[1].length() > 0) {
        string1 = string1 + '\n' + stringArray1[1];
    }
    return string1;
}

bool Extension::isSafeZone(int32_t dungeonId, int32_t x, int32_t y) {
    if (dungeonId != 1) {
        return false;
    }
    if (x < 6 || x > 12) {
        return false;
    }
    return y >= 6 && y <= 12;
}

bool Extension::enchantItem(Player &player, int32_t n) {
    int8_t by1 = player.inventory_[n];
    if (Items::isWeaponOrArmour(by1 = (int8_t)wrappingAbs(by1))) {
        int32_t n1 = player.itemData_[n] & 0xFF;
        n1 = 3;
        int32_t n2 = n;
        player.itemData_[n2] = player.itemData_[n2] & 0xFFFFFF00;
        int32_t n3 = n;
        player.itemData_[n3] = player.itemData_[n3] | n1;
        return true;
    }
    return false;
}

bool Extension::isEnchanted(Player &player, int32_t n) {
    int8_t by1 = player.inventory_[n];
    by1 = (int8_t)wrappingAbs(by1);
    (void)by1;
    int8_t by2 = (int8_t)(player.itemData_[n] & 0xFF);
    return by2 == 3;
}

int32_t Extension::itemTier(Player &player, int32_t n) {
    int8_t by1 = player.inventory_[n];
    by1 = (int8_t)wrappingAbs(by1);
    return Items::at(by1 - 1).tier;
}

std::string Extension::describeRecord(const SharedArray<int8_t> &byArray) {
    std::string string1 = "X = " + std::to_string((int32_t)byArray[0]) + '\n';
    string1 += "Y = " + std::to_string((int32_t)byArray[1]) + '\n';
    string1 += "Type = " + std::to_string((int32_t)byArray[2]) + '\n';
    string1 += "ID(MSB) = " + std::to_string((int32_t)byArray[3]) + '\n';
    string1 += "ID(LSB) = " + std::to_string((int32_t)byArray[4]) + '\n';
    string1 += "value = " + std::to_string((int32_t)byArray[5]) + '\n';
    string1 += "flags = " + std::to_string((int32_t)byArray[6]) + '\n';
    return string1;
}

void Extension::turnLeft(Player &player) {
    player.facing_ = (int8_t)(player.facing_ - 1);
    if (player.facing_ <= 0) {
        player.facing_ = (int8_t)4;
    }
}

void Extension::turnRight(Player &player) {
    player.facing_ = (int8_t)(player.facing_ + 1);
    if (player.facing_ >= 5) {
        player.facing_ = 1;
    }
}

std::string Extension::debugDump(Player &player) {
    int8_t by1 = player.facing_;
    std::string out;
    out.reserve(1000);
    worldstate::WorldState &worldState = player.world_->worldState();
    SharedArray<std::string> stringArray1 = dungeonOf(player)->name();
    out += "Current dungeon is ";
    out += stringArray1[0];
    out += ' ';
    out += stringArray1[1];
    out += "\n";
    int32_t n1 = 1;
    while (n1 <= 4) {
        SharedArray<int8_t> byArray1;
        if (n1 <= 2) {
            player.stepCandidate(n1);
        } else if (n1 == 3) {
            turnRight(player);
            player.stepCandidate(1);
        } else if (n1 == 4) {
            turnLeft(player);
            player.stepCandidate(1);
        }
        if (n1 == 1) {
            out += "FORWARD SQUARE: \n";
        } else if (n1 == 2) {
            out += "BACKWARD SQUARE: \n";
        } else if (n1 == 3) {
            out += "RIGHT SIDE SQUARE: \n";
        } else if (n1 == 4) {
            out += "LEFT SIDE SQUARE: \n";
        }
        out += "x,y = ";
        out += std::to_string((int32_t)player.nextX_);
        out += ", ";
        out += std::to_string((int32_t)player.nextY_);
        out += "\n";
        out += "map value = ";
        out += std::to_string((int32_t)dungeonOf(player)->tiles_[player.nextX_][player.nextY_]);
        out += "\n";
        const std::size_t monsterDungeonIndex = (std::size_t)(player.nextDungeon_ - 1);
        if (worldState.monsters.hasTable(monsterDungeonIndex)) {
            for (const SharedArray<int8_t> &record : worldState.monsters.at(monsterDungeonIndex)) {
                byArray1 = record;
                if (byArray1[4] != player.nextX_ || byArray1[5] != player.nextY_) continue;
                out += "Found monster in square \ntype=";
                out += std::to_string((int32_t)byArray1[2]);
                out += ", health=";
                out += std::to_string((int32_t)byArray1[3]);
                out += ", dungeon id = ";
                out += std::to_string((int32_t)byArray1[7]);
                out += "\n";
            }
        }
        const std::size_t chestDungeonIndex = (std::size_t)(player.nextDungeon_ - 1);
        if (worldState.chests.hasTable(chestDungeonIndex)) {
            for (const SharedArray<int8_t> &chest : worldState.chests.at(chestDungeonIndex)) {
                byArray1 = chest;
                if (byArray1[0] != player.nextX_ || byArray1[1] != player.nextY_) continue;
                out += "Found chest in square \nitem type=";
                out += std::to_string((int32_t)byArray1[4]);
                out += ", value=";
                out += std::to_string((int32_t)byArray1[7]);
                out += "\n";
            }
        }
        for (const SharedArray<int8_t> &dropped :
             worldState.droppedItems.at((std::size_t)(player.nextDungeon_ - 1))) {
            byArray1 = dropped;
            if (byArray1[0] != player.nextX_ || byArray1[1] != player.nextY_) continue;
            out += "Found dropped item in square \nitem type=";
            out += std::to_string((int32_t)byArray1[2]);
            out += ", value=";
            out += std::to_string((int32_t)byArray1[5]);
            out += " flags = ";
            out += std::to_string((int32_t)byArray1[6]);
            out += "\n";
        }
        player.facing_ = by1;
        ++n1;
    }
    if (worldState.npcs.wardenPresent) {
        out += "Warden IS visiting now\n";
    } else {
        out += "Warden IS NOT visiting now\n";
    }
    out += "Player inventory: nitems=";
    out += std::to_string((int32_t)player.itemCount_);
    return std::string(out);
}

namespace {

Game &gameOf(Player &player) { return *static_cast<Game *>(player.world_); }

}

int32_t Extension::runMonsters(GameCanvas &canvas, int64_t l) {
    Player &player = *canvas.player_;
    worldstate::WorldState &worldState = player.world_->worldState();
    const std::size_t dungeonIndex = (std::size_t)(player.dungeonId_ - 1);
    if (!worldState.monsters.hasTable(dungeonIndex)) {
        return 0;
    }
    const worldstate::MonsterList records = worldState.monsters.at(dungeonIndex);
    Monster decodedMonster;
    for (const SharedArray<int8_t> &byArray1 : records) {
        Monster *d2 = Monster::fromRecord(&decodedMonster, byArray1, dungeonOf(player));
        if (d2->isAdjacent(&player)) {
            if (d2->attackPhase_ == 0) {
                d2->lastActionMs_ = l;
                d2->attackPhase_ = 1;
            } else if (d2->attackPhase_ == 1 && l - d2->lastActionMs_ > 800L) {
                d2->attack(&player, l);
                player.world_->platformContext()->playSound(platform::Sound::PlayerHurt);
                canvas.postMessage(GameCanvas::msgCreatureAttacks_, 2, l);
            } else if (l - d2->lastActionMs_ > 800L) {
                d2->attack(&player, l);
                player.world_->platformContext()->playSound(platform::Sound::PlayerHurt);
            }
            d2->store();
            continue;
        }
        d2->takeTurn(&player);
        d2->store();
    }
    return 0;
}

void Extension::onCampAmbush(GameCanvas &canvas) {
    dungeonOf(canvas.player_)->spawnNear(canvas.player_);
}

void Extension::beforeWorldTick(GameCanvas &canvas) {
    Player &player = *canvas.player_;
    Game &game = gameOf(player);
    worldstate::NpcState &npcs = player.world_->worldState().npcs;
    if (NpcSystem::wardenDue(npcs, (int32_t)player.giftPoints_)) {
        NpcSystem::wardenArrive(npcs, game.dungeons_);
    }
    if (this->inWardensCamp(canvas) && npcs.wardenVisits > wardenStage_) {
        std::string string1 = NpcSystem::interact(&player, 6, -1, -1).value_or("");
        Variant &variant = variantOf(player);
        variant.wardenSpeaksUI_ = variant.newWardenSpeaksUI(string1);
        this->wardenHandoffPending_ = true;
    }
}

void Extension::afterTick(GameCanvas &canvas) {
    if (this->wardenHandoffPending_) {
        this->wardenHandoffPending_ = false;
        canvas.pauseForUi();
        Game &game = gameOf(*canvas.player_);
        game.setCurrentDisplay(variantOf(game).wardenSpeaksUI_);
    }
}

void Extension::onNpcAhead(GameCanvas &canvas, bool npcTileAhead) {
    if (npcTileAhead) {
        return;
    }
    Player &player = *canvas.player_;
    worldstate::NpcState &npcs = player.world_->worldState().npcs;
    if (!this->inWardensCamp(canvas) && npcs.wardenPresent && wardenStage_ == npcs.wardenVisits) {
        NpcSystem::wardenLeave(npcs, gameOf(player).dungeons_);
    }
}

bool Extension::inWardensCamp(const GameCanvas &canvas) const {
    Player &player = *canvas.player_;
    if (NpcSystem::isWardenAdjacent(&player)) {
        return true;
    }
    Monster *target = canvas.combatMonster_;
    if (target == nullptr) {
        return false;
    }
    if (player.dungeonId_ == 37 && target->type_ == 41) {
        int32_t n1;
        int32_t n2 = wrappingAbs(player.gridX_ - target->gridX_);
        return n2 + (n1 = wrappingAbs(player.gridY_ - target->gridY_)) == 1;
    }
    return false;
}

void Extension::onMonsterSlain(GameCanvas &canvas, Monster &monster) {
    (void)canvas;
    if (monster.type_ == 41) {
        monster.dropLoot(true);
    } else {
        monster.dropLoot(false);
    }
}

void Extension::openNpcScreen(GameCanvas &canvas, int32_t n) {
    variantOf(*canvas.player_).openNpcScreen(canvas, n);
}

bool Extension::crossedBoundary(const Player &player) {
    return player.crossedEdge_ || enteredSafeZone_ || leftSafeZone_;
}

SharedArray<std::string> Extension::arrivalMessage(GameCanvas &canvas) {
    if (enteredSafeZone_) {
        return SharedArray<std::string>{std::string("Warden's"), std::string("Camp")};
    }
    if (leftSafeZone_) {
        return SharedArray<std::string>{std::string("Outer"), std::string("Camp")};
    }
    return dungeonOf(canvas.player_)->name();
}

bool Extension::talkLayoutForNpc(const GameCanvas &canvas) {
    return canvas.npcTileAhead_ && !this->inWardensCamp(canvas);
}

std::string Extension::npcName(int32_t npc) {
    return NpcSystem::npcNames_[npc];
}

void Extension::drawNpcSlot(GameCanvas &canvas, Graphics *graphics, const std::string &object2,
                            int32_t n) {
    if (object2 != "W") {
        return;
    }
    if (n >= 8) {
        canvas.drawMonsterFar(graphics, 32, n);
        return;
    }
    int32_t n1 = 0;
    int32_t n5 = 0;
    switch (n) {
        case 4: {
            n1 = 10;
            n5 = 38;
            break;
        }
        case 5: {
            n1 = 62;
            n5 = 38;
            break;
        }
        case 6: {
            n1 = 112;
            n5 = 38;
        }
    }
    canvas.drawFrame(graphics, GameCanvas::npcSprites_[31], 0, 1, n1, n5);
}

int32_t Extension::wallKind(int8_t tile) {
    return GameUtil::hasFlag((int8_t)1, tile) ? 1 : 0;
}

void Extension::drawWallSlice(GameCanvas &canvas, Graphics *graphics, int32_t n, int32_t n2,
                              int32_t kind) {
    (void)kind;
    graphics->setClip(n2, 0, 18, canvas.getHeight());
    if (n > 7) {
        int32_t n1 = n - 8;
        graphics->drawImage(GameCanvas::wallRightImage_, n2 - n1 * 18, 0, 20,
                            IMAGE_FLIP_HORIZONTAL);
    } else {
        graphics->drawImage(GameCanvas::wallRightImage_, n2 - n * 18, 0, 20);
    }
    graphics->setClip(0, 0, canvas.getWidth(), canvas.getHeight());
}

bool Extension::isCrystal(const SharedArray<int8_t> &byArray) {
    if (byArray.length() != 7) {
        return false;
    }
    return (byArray[6] & 4) != 0;
}

void Extension::drawObject(GameCanvas &canvas, Graphics *graphics, const SharedArray<int8_t> &byArray,
                           int32_t n) {
    (void)canvas;
    if (n == 1) {
        if (isCrystal(byArray)) {
            int32_t n1 = 45;
            int32_t n2 = 65;
            GameCanvas::crystalSprites_[0]->draw(graphics, n1, n2);
        } else {
            int32_t n3 = 60;
            int32_t n4 = 94;
            int32_t n5 = 0;
            if (byArray.length() == 8) {
                GameCanvas::chestSprites_[n5]->draw(graphics, n3, n4);
            } else if (byArray.length() == 7) {
                GameCanvas::bagSprites_[n5]->draw(graphics, n3, n4 += 14);
            }
        }
    } else if (n >= 4 && n <= 6) {
        int32_t n1 = 1;
        int32_t n2 = 0;
        int32_t n3 = 0;
        bool bl1 = isCrystal(byArray);
        switch (n) {
            case 4: {
                n2 = 14;
                n3 = 80;
                if (!bl1) break;
                n2 = 14;
                n3 = 55;
                break;
            }
            case 5: {
                n2 = 68;
                n3 = 80;
                if (bl1) {
                    n2 = 73;
                    n3 = 55;
                    break;
                }
                if (byArray.length() != 7) break;
                n2 = 73;
                n3 = 80;
                break;
            }
            case 6: {
                n2 = 122;
                n3 = 80;
                if (bl1) {
                    n2 = 125;
                    n3 = 55;
                    break;
                }
                if (byArray.length() != 7) break;
                n2 = 132;
                n3 = 80;
            }
        }
        if (bl1) {
            GameCanvas::crystalSprites_[n1]->draw(graphics, n2, n3 += 13);
        } else if (byArray.length() == 8) {
            GameCanvas::chestSprites_[n1]->draw(graphics, n2, n3 += 17);
        } else if (byArray.length() == 7) {
            GameCanvas::bagSprites_[n1]->draw(graphics, n2, n3 += 20);
        }
    } else if (n >= 8 && n <= 12) {
        int32_t n1 = 2;
        int32_t n2 = 0;
        int32_t n3 = 0;
        bool bl1 = isCrystal(byArray);
        switch (n) {
            case 8: {
                n2 = 10;
                n3 = 59;
                if (!bl1) break;
                n2 = 10;
                n3 = 52;
                break;
            }
            case 9: {
                n2 = 44;
                n3 = 59;
                if (!bl1) break;
                n2 = 44;
                n3 = 52;
                break;
            }
            case 10: {
                n2 = 79;
                n3 = 59;
                if (!bl1) break;
                n2 = 79;
                n3 = 52;
                break;
            }
            case 11: {
                n2 = 112;
                n3 = 59;
                if (!bl1) break;
                n2 = 112;
                n3 = 52;
                break;
            }
            case 12: {
                n2 = 146;
                n3 = 59;
                if (!bl1) break;
                n2 = 146;
                n3 = 52;
            }
        }
        if (bl1) {
            GameCanvas::crystalSprites_[n1]->draw(graphics, n2, n3 += 20);
        } else if (byArray.length() == 8) {
            GameCanvas::chestSprites_[n1]->draw(graphics, n2, n3 += 28);
        } else if (byArray.length() == 7) {
            GameCanvas::bagSprites_[n1]->draw(graphics, n2, n3 += 28);
        }
    }
}

void Extension::drawWardenPortrait(GameCanvas &canvas, Graphics *graphics, int32_t n) {
    int32_t n1 = 15;
    int32_t n2 = 32;
    // Slot 28 is the Warden's body, and it is only resident while the camp art
    // is loaded.  drawFrame tolerates a missing sprite; the width read below
    // does not, so there is nothing to mirror if the art is not there.
    if (GameCanvas::npcSprites_[28] == nullptr) {
        return;
    }
    canvas.drawFrame(graphics, GameCanvas::npcSprites_[28], GameCanvas::kHudIconRows[n][0], 1, n1,
                     n2);
    int32_t n3 = GameCanvas::npcSprites_[28]->width();
    canvas.drawFrame(graphics, GameCanvas::npcSprites_[28], GameCanvas::kHudIconRows[n][0], 1,
                     n1 + n3, n2, 8192);
    canvas.drawFrame(graphics, GameCanvas::npcSprites_[29], GameCanvas::kHudIconRows[n][1], 3,
                     n1 + 45, n2 + -22);
}

bool Extension::drawSpecialMonster(GameCanvas &canvas, Graphics *graphics, int32_t type) {
    if (type != 41) {
        return false;
    }
    this->drawWardenPortrait(canvas, graphics, 2);
    return true;
}

void Extension::drawNpcPortrait(GameCanvas &canvas, Graphics *graphics, int32_t n) {
    switch (n) {
        case 0: {
            canvas.drawMonsterNear(graphics, 1, 1);
            break;
        }
        case 1: {
            canvas.drawMonsterNear(graphics, 6, 1);
            break;
        }
        case 2: {
            canvas.drawMonsterNear(graphics, 7, 1);
            break;
        }
        case 3: {
            canvas.drawMonsterNear(graphics, 2, 1);
            break;
        }
        case 5: {
            canvas.drawMonsterNear(graphics, 8, 0);
            break;
        }
        case 4: {
            canvas.drawMonsterNear(graphics, 3, 2);
            break;
        }
        case 6: {
            // wardenVisits is 0 before he has ever turned up, and kHudIconRows
            // has no row -1 to describe how to draw him.
            int32_t n1 = min32(canvas.player_->world_->worldState().npcs.wardenVisits, 3) - 1;
            if (n1 >= 0) {
                this->drawWardenPortrait(canvas, graphics, n1);
            }
        }
    }
    graphics->setClip(0, 0, canvas.getWidth(), canvas.getHeight());
}

void Extension::drawIconRow(GameCanvas &canvas, Graphics *graphics, int32_t n1) {
    const char *labels = canvas.iconLabels();
    graphics->setFont(GameCanvas::hudFont_);
    graphics->setClip(0, 0, canvas.getWidth(), canvas.getHeight());
    graphics->setColor(0);
    graphics->fillRect(0, 156, canvas.getWidth(), 52);
    graphics->setColor(13080935);
    graphics->fillRoundRect(2, 158, canvas.getWidth() - 4, 48, 5, 5);
    graphics->setColor(0);
    if (n1 == 0) {
        graphics->drawImage(GameCanvas::hudIcons_[1], 14, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[2], 62, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[3], 104, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[5], 144, 174, 20);
        graphics->drawChar(labels[1], 5, 180, 20);
        graphics->drawChar(labels[2], 53, 180, 20);
        graphics->drawChar(labels[3], 96, 180, 20);
        graphics->drawChar(labels[5], 135, 180, 20);
    } else if (n1 == 1) {
        graphics->drawImage(GameCanvas::hudIcons_[0], 14, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[1], 62, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[2], 104, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[3], 144, 174, 20);
        graphics->drawChar(labels[0], 5, 180, 20);
        graphics->drawChar(labels[1], 53, 180, 20);
        graphics->drawChar(labels[2], 96, 180, 20);
        graphics->drawChar(labels[3], 135, 180, 20);
    } else if (n1 == 2) {
        graphics->drawImage(GameCanvas::hudIcons_[1], 14, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[2], 62, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[3], 104, 174, 20);
        graphics->drawImage(GameCanvas::hudIcons_[4], 144, 174, 20);
        graphics->drawChar(labels[1], 5, 180, 20);
        graphics->drawChar(labels[2], 53, 180, 20);
        graphics->drawChar(labels[3], 96, 180, 20);
        graphics->drawChar(labels[4], 135, 180, 20);
    }
}

void Extension::drawEffect(GameCanvas &canvas, Graphics *graphics, int32_t effect, int32_t x,
                           int32_t y) {
    (void)canvas;
    graphics->drawImage(GameCanvas::effectImages_[effect], x, y, 20);
}

void Extension::refreshMinimap(GameCanvas &canvas) {
    Player &player = *canvas.player_;
    int8_t by1 = player.gridX_;
    int8_t by2 = player.gridY_;
    int8_t by3 = player.facing_;
    dungeonOf(player)->c(by1, by2, by3, canvas.mapGrid7_);
    if (canvas.mapMode_ == 2) {
        dungeonOf(player)->a((int32_t)by1, (int32_t)by2, (int32_t)by3, canvas.mapGrid17_);
    }
}

void Extension::invalidateMinimap(GameCanvas &canvas) {
    this->refreshMinimap(canvas);
}

void Extension::drawMinimap(GameCanvas &canvas, Graphics *graphics) {
    if (canvas.mapMode_ == 1) {
        graphics->setColor(0);
        graphics->fillRect(10, 20, 23, 23);
        minimap::plot(graphics, canvas.mapGrid7_, 10 + 1, 20 + 1, 7, 3);
    } else {
        int32_t n1 = 89;
        graphics->fillRect(15, 25, n1, n1);
        minimap::plot(graphics, canvas.mapGrid17_, 15 + 2, 25 + 2, 17, 5);
    }
}

}
