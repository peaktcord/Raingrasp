#include "src/dawnstar/extension.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/util.hpp"
#include "src/common/render/minimap.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/dawnstar/variant.hpp"
#include "src/dawnstar/npc_script.hpp"

namespace dawnstar {

int32_t Extension::skillRankBonus(const Player &player) {
    (void)player;
    return tenacity_ ? 4 : 0;
}

void Extension::onInitFromClass(Player &player) {
    this->traitorId_ = (int8_t)(GameUtil::randomInt(player.world_->worldState().random, 4) - 1);
    platform::writeLogLine("World: traitor is npc " + std::to_string((int32_t)this->traitorId_));
}

void Extension::onPlaceAtStart(Player &player) {
    this->removeRoamer(player);
}

void Extension::onEdgeCandidate(Player &player) {
    this->removeRoamer(player);
}

void Extension::onRest(Player &player) {
    this->removeRoamer(player);
}

void Extension::removeRoamer(Player &player) {
    if (!this->roamerActive_) {
        return;
    }
    worldstate::WorldState &worldState = player.world_->worldState();
    const std::size_t dungeonIndex = (std::size_t)(player.dungeonId_ - 1);
    if (worldState.monsters.hasTable(dungeonIndex)) {
        Monster *d2 = new Monster();
        for (const SharedArray<int8_t> &byArray1 : worldState.monsters.at(dungeonIndex)) {
            Monster::fromRecord(d2, byArray1, player.world_->dungeonAt(player.dungeonId_));
            if (d2->type_ != 41) continue;
            this->roamerActive_ = false;
            player.world_->removeMonsterAt(player.dungeonId_, d2->gridX_, d2->gridY_);
            break;
        }
    }
    if (this->roamerActive_) {
        platform::writeLogLine("ERROR: failed to remove the roaming monster");
    }
}

bool Extension::takeItemsUnderfoot(Player &player, int8_t tile, bool forward) {
    (void)tile;
    if (!forward) {
        return false;
    }
    Dungeon *i2 = dungeonOf(player);
    bool bl2 = true;
    worldstate::DroppedItemList items = i2->droppedItemsAt(player.gridX_, player.gridY_);
    for (const SharedArray<int8_t> &byArray1 : items) {
        bool bl3 = player.takeDroppedItem(byArray1);
        if (bl3) {
            int32_t n1;
            i2->removeDroppedItem(byArray1);
            if ((byArray1[6] & 2) != 0 || Items::at(n1 = byArray1[2] - 1).category != 11) continue;
            player.giftPoints_ = (int16_t)(player.giftPoints_ + (int16_t)Items::at(n1).tier);
            continue;
        }
        bl2 = false;
    }
    if (bl2) {
        i2->clearItemBit((int32_t)player.gridX_, (int32_t)player.gridY_);
    }
    return false;
}

void Extension::placeVisibleNpcs(Player &player) {
    if (player.dungeonId_ == 1) {
        int32_t n1 = 0;
        while (n1 < 5) {
            player.placeVisible(6, visibility::Slot(NpcSystem::npcNames_[n1]));
            ++n1;
        }
    } else if (player.dungeonId_ == 3) {
        player.placeVisible(6, visibility::Slot(std::string("A")));
    } else if (player.dungeonId_ == 12) {
        player.placeVisible(6, visibility::Slot(std::string("B")));
    } else if (player.dungeonId_ == 21) {
        player.placeVisible(6, visibility::Slot(std::string("C")));
    } else if (player.dungeonId_ == 30) {
        player.placeVisible(6, visibility::Slot(std::string("D")));
    }
}

bool Extension::npcPosition(const Player &player, int32_t kind, const visibility::Slot &object, int8_t &x,
                            int8_t &y) {
    if (kind != 6) {
        return false;
    }
    if (player.dungeonId_ == 1) {
        std::string string1 = *object.npc();
        int32_t n2 = 0;
        while (n2 < 5) {
            if (string1 == NpcSystem::npcNames_[n2]) {
                x = NpcSystem::npcGridX_[n2];
                y = NpcSystem::npcGridY_[n2];
                n2 = 5;
            }
            ++n2;
        }
    } else if (player.dungeonId_ == 3) {
        x = NpcSystem::npcGridX_[5];
        y = NpcSystem::npcGridY_[5];
    } else if (player.dungeonId_ == 12) {
        x = NpcSystem::npcGridX_[6];
        y = NpcSystem::npcGridY_[6];
    } else if (player.dungeonId_ == 21) {
        x = NpcSystem::npcGridX_[7];
        y = NpcSystem::npcGridY_[7];
    } else if (player.dungeonId_ == 30) {
        x = NpcSystem::npcGridX_[8];
        y = NpcSystem::npcGridY_[8];
    }
    return true;
}

int32_t Extension::npcAhead(const Player &player, int32_t dungeonId, int32_t x, int32_t y) {
    (void)player;
    if (dungeonId == 3 && x == NpcSystem::npcGridX_[5] && y == NpcSystem::npcGridY_[5]) {
        return 5;
    }
    if (dungeonId == 12 && x == NpcSystem::npcGridX_[6] && y == NpcSystem::npcGridY_[6]) {
        return 6;
    }
    if (dungeonId == 21 && x == NpcSystem::npcGridX_[7] && y == NpcSystem::npcGridY_[7]) {
        return 7;
    }
    if (dungeonId == 30 && x == NpcSystem::npcGridX_[8] && y == NpcSystem::npcGridY_[8]) {
        return 8;
    }
    if (dungeonId != 1) {
        return -1;
    }
    return NpcSystem::npcAt(x, y);
}

std::string Extension::itemEffectText(int32_t index) {
    return dawnstar::itemEffectText[index];
}

void Extension::giveStarFrost(Player &player) {
    this->tenacity_ = true;
    int16_t s1 = Items::nextId();
    bool bl1 = player.addItem(100, s1, 0);
    if (!bl1) {
        int32_t n1 = 10000;
        int32_t n2 = -1;
        int32_t n3 = -1;
        int32_t n4 = 0;
        while (n4 < player.itemCount_) {
            int32_t n5 = wrappingAbs(player.inventory_[n4]);
            if (n5 == 87) {
                n3 = n4;
                break;
            }
            int32_t n6 = Items::stat(5, n5);
            if (!player.isEquipped(n4) && n6 < n1 && n6 > 0) {
                n2 = n4;
                n1 = n6;
            }
            ++n4;
        }
        if (n3 > -1) {
            player.removeItem(n3);
        } else {
            player.removeItem(n2);
        }
        bl1 = player.addItem(100, s1, 0);
        if (!bl1) {
            platform::writeLogLine("ERROR: failed to add the StarFrost item to the world");
        }
    }
}

namespace {

Game &gameOf(Player &player) { return *static_cast<Game *>(player.world_); }

}

const int32_t Extension::kOracleOffsets[12] = {0, 0, 36, 72, 90, 108, 126, 144, 158, 176, 194, 212};

int32_t Extension::runMonsters(GameCanvas &canvas, int64_t nowMs) {
    return dungeonOf(canvas.player_)->runMonsters(nowMs, canvas.player_);
}

int8_t Extension::campStateFor(GameCanvas &canvas, int8_t state) {
    Player &player = *canvas.player_;
    if (!bossKilled_ && player.vitals_[0] > 3 &&
        GameUtil::randomInt(player.world_->worldState().random, 10) == 1) {
        return (int8_t)3;
    }
    return state;
}

void Extension::onCampAmbush(GameCanvas &canvas) {
    Player &player = *canvas.player_;
    if (canvas.campState_ == 3) {
        roamerActive_ = true;
        if (!dungeonOf(player)->spawnNear(player.gridX_, player.gridY_, 41)) {
        }
    } else {
        dungeonOf(player)->spawnNear((int32_t)player.gridX_, (int32_t)player.gridY_, -1);
    }
}

void Extension::onRespawn(GameCanvas &canvas) {
    (void)canvas;
    tenacity_ = false;
}

void Extension::onMonsterSlain(GameCanvas &canvas, Monster &monster) {
    if (monster.type_ == 41) {
        bossKilled_ = true;
        roamerActive_ = false;
    }
    if (monster.type_ == 42) {
        gameOf(*canvas.player_).showEndOfGame();
        // The end-of-game screen is already on display by the time we return, so stop the
        // canvas from painting one more dungeon frame over it. Without this the wide-view
        // side panels rebuild from the post-switch state and flash for a frame.
        canvas.repaintEnabled_ = false;
    } else {
        monster.dropLoot(false);
    }
}

void Extension::openNpcScreen(GameCanvas &canvas, int32_t n) {
    variantOf(*canvas.player_).openNpcScreen(canvas, n);
}

bool Extension::crossedBoundary(const Player &player) {
    return player.crossedEdge_;
}

SharedArray<std::string> Extension::arrivalMessage(GameCanvas &canvas) {
    return canvas.messageLinesFor(dungeonOf(canvas.player_)->name(),
                                  GameCanvas::LineRule::Whole);
}

bool Extension::talkLayoutForNpc(const GameCanvas &canvas) {
    return canvas.npcAhead_ >= 0;
}

void Extension::onSecond(GameCanvas &canvas) {
    if (oracleIndex_ < 0) {
        return;
    }
    Player &player = *canvas.player_;
    worldstate::WorldState &worldState = player.world_->worldState();
    int32_t n4 = ++oracleIndex_;
    int32_t n5 = -1;
    if (traitorRevealed_) {
        switch (n4) {
            case 3: {
                n5 = 4;
                break;
            }
            case 20: {
                n5 = 16;
                break;
            }
            case 35: {
                n5 = 7;
                break;
            }
            case 38: {
                n5 = 18;
                break;
            }
            case 53: {
                n5 = 12;
                break;
            }
            case 68: {
                n5 = 20;
                break;
            }
            case 70: {
                n5 = 22;
                break;
            }
            case 85: {
                n5 = 24;
                break;
            }
            case 100: {
                n5 = 26;
                break;
            }
            case 115: {
                n5 = 28;
                break;
            }
            case 117: {
                n5 = 30;
                break;
            }
            case 127: {
                n5 = 31;
                break;
            }
            case 132: {
                n5 = 32;
            }
        }
    } else {
        switch (n4) {
            case 5: {
                n5 = 4;
                break;
            }
            case 16: {
                n5 = 16;
                break;
            }
            case 28: {
                n5 = 8;
                break;
            }
            case 38: {
                n5 = 20;
                break;
            }
            case 42: {
                n5 = 21;
                break;
            }
            case 55: {
                n5 = 22;
                break;
            }
            case 66: {
                n5 = 23;
                break;
            }
            case 77: {
                n5 = 24;
                break;
            }
            case 88: {
                n5 = 25;
                break;
            }
            case 99: {
                n5 = 26;
                break;
            }
            case 105: {
                n5 = 27;
                break;
            }
            case 115: {
                n5 = 28;
                break;
            }
            case 118: {
                n5 = 29;
                break;
            }
            case 127: {
                n5 = 30;
                break;
            }
            case 132: {
                n5 = 31;
            }
        }
    }
    if (n4 == 140) {
        n5 = 42;
    }
    if (n5 > 0) {
        int32_t n6 = 1 + GameUtil::randomInt(worldState.random, 17);
        int32_t n7 = 1 + GameUtil::randomInt(worldState.random, 17);
        while (!dungeonOf(player)->spawnNear(n6, n7, n5)) {
            n6 = 1 + GameUtil::randomInt(worldState.random, 17);
            n7 = 1 + GameUtil::randomInt(worldState.random, 17);
        }
        if (worldState.monsters.at((std::size_t)(player.dungeonId_ - 1)).size() > 5) {
            variantOf(player).showGameOver();
        } else {
            canvas.postMessage(SharedArray<std::string>{"Enemy", "arrived!"}, 3,
                               player.world_->platformContext()->nowMillis());
        }
    }
}

std::string Extension::npcName(int32_t npc) {
    return NpcSystem::npcNames_[npc];
}

void Extension::drawNpcSlot(GameCanvas &canvas, Graphics *graphics, const std::string &object2,
                            int32_t n) {
    const bool far = n >= 8;
    int32_t sprite;
    if (object2 == "W") {
        sprite = far ? 25 : 24;
    } else if (object2 == "C" || object2 == "D" ||
               object2 == NpcSystem::npcNames_[0] || object2 == NpcSystem::npcNames_[1]) {
        sprite = far ? 6 : 5;
    } else {
        sprite = far ? 13 : 12;
    }
    if (far) {
        canvas.drawMonsterFar(graphics, sprite, n);
    } else {
        canvas.drawMonsterMid(graphics, sprite, n);
    }
}

int32_t Extension::wallKind(int8_t tile) {
    if (GameUtil::hasFlag((int8_t)1, tile)) {
        return 1;
    }
    if (GameUtil::hasFlag((int8_t)64, tile)) {
        return 2;
    }
    return 0;
}

void Extension::beginWalls(GameCanvas &canvas) {
    (void)canvas;
    sideWallPending_ = false;
    frontWallPending_ = false;
}

void Extension::drawWallImage(Graphics *graphics, int32_t n3, int32_t x) {
    if (n3 == -1) {
        graphics->drawImage(GameCanvas::gateImage_, x, 8, 20);
    } else if (n3 != 1) {
        graphics->drawImage(GameCanvas::wallInnerImage_, x, 0, 20);
    } else {
        graphics->drawImage(GameCanvas::wallRightImage_, x, 0, 20);
    }
}

void Extension::drawWallSlice(GameCanvas &canvas, Graphics *graphics, int32_t n, int32_t n2,
                              int32_t kind) {
    const int32_t n3 = kind == 2 ? -1 : (int32_t)canvas.player_->dungeon()->id_;
    graphics->setClip(n2, 0, 18, canvas.getHeight());
    if (n == 0 || n == 1) {
        this->frontWallPending_ = false;
        if (this->sideWallPending_) {
            this->sideWallPending_ = false;
            drawWallImage(graphics, n3, n2 - kOracleOffsets[n] - 18);
            return;
        }
        this->sideWallPending_ = true;
    } else {
        this->sideWallPending_ = false;
        if (n == 2) {
            if (this->frontWallPending_) {
                this->frontWallPending_ = false;
                drawWallImage(graphics, n3, n2 - kOracleOffsets[n] - 18);
                return;
            }
            this->frontWallPending_ = true;
        }
    }
    drawWallImage(graphics, n3, n2 - kOracleOffsets[n]);
}

void Extension::drawObject(GameCanvas &canvas, Graphics *graphics, const SharedArray<int8_t> &record,
                           int32_t n) {
    (void)canvas;
    const bool bl = record.length() == 8;
    if (n == 1) {
        int32_t n1 = 60;
        int32_t n2 = 110;
        if (bl) {
            GameCanvas::chestSprites_[0]->draw(graphics, n1, n2);
        } else {
            GameCanvas::bagSprites_[0]->draw(graphics, n1, n2 += 14);
        }
    } else if (n >= 4 && n <= 6) {
        int32_t n1 = 0;
        int32_t n2 = 97;
        switch (n) {
            case 4: {
                n1 = 14;
                break;
            }
            case 5: {
                if (bl) {
                    n1 = 68;
                    break;
                }
                n1 = 73;
                break;
            }
            case 6: {
                n1 = bl ? 125 : 142;
            }
        }
        if (bl) {
            GameCanvas::chestSprites_[1]->draw(graphics, n1, n2);
        } else {
            GameCanvas::bagSprites_[1]->draw(graphics, n1, n2 += 8);
        }
    } else if (n >= 8 && n <= 12) {
        int32_t n1 = 0;
        int32_t n2 = 87;
        switch (n) {
            case 8: {
                n1 = 10;
                break;
            }
            case 9: {
                n1 = 46;
                break;
            }
            case 10: {
                n1 = 84;
                break;
            }
            case 11: {
                n1 = 120;
                break;
            }
            case 12: {
                n1 = 156;
            }
        }
        if (bl) {
            GameCanvas::chestSprites_[2]->draw(graphics, n1, n2);
        } else {
            GameCanvas::bagSprites_[2]->draw(graphics, n1, n2);
        }
    }
}

bool Extension::drawSpecialMonster(GameCanvas &canvas, Graphics *graphics, int32_t type) {
    if (type != 41 && type != 42) {
        return false;
    }
    int32_t n1 = 33;
    int32_t n2 = 48;
    canvas.drawFrame(graphics, GameCanvas::npcSprites_[23], type == 41 ? 0 : 1, 2, n1, n2);
    graphics->setClip(0, 0, canvas.getWidth(), canvas.getHeight());
    return true;
}

void Extension::drawNpcPortrait(GameCanvas &canvas, Graphics *graphics, int32_t n) {
    switch (n) {
        case 0: {
            canvas.drawMonsterNear(graphics, 1, 2);
            break;
        }
        case 2: {
            canvas.drawMonsterNear(graphics, 7, 0);
            break;
        }
        case 1: {
            canvas.drawMonsterNear(graphics, 4, 1);
            break;
        }
        case 3: {
            canvas.drawMonsterNear(graphics, 6, 2);
            break;
        }
        case 4: {
            canvas.drawMonsterNear(graphics, 8, 1);
            break;
        }
        case 5: {
            canvas.drawMonsterNear(graphics, 9, 0);
            break;
        }
        case 6: {
            canvas.drawMonsterNear(graphics, 10, 2);
            break;
        }
        case 7: {
            canvas.drawMonsterNear(graphics, 2, 0);
            break;
        }
        case 8: {
            canvas.drawMonsterNear(graphics, 3, 2);
        }
    }
}

void Extension::drawIcon(Graphics *graphics, int32_t n, int32_t n2, int32_t n3) {
    graphics->setClip(n2, n3, 30, 24);
    graphics->drawImage(GameCanvas::hudIconAtlas_, n2 - 30 * n, n3, 20);
}

void Extension::drawIconRow(GameCanvas &canvas, Graphics *graphics, int32_t n1) {
    const char *labels = canvas.iconLabels();
    graphics->setFont(GameCanvas::hudFont_);
    graphics->setClip(0, 0, canvas.getWidth(), canvas.getHeight());
    graphics->drawImage(GameCanvas::hudPanelImage_, 0, 156, 20);
    graphics->setColor(0);
    if (n1 == 0) {
        graphics->drawChar(labels[1], 26, 191, 20);
        graphics->drawChar(labels[2], 66, 191, 20);
        graphics->drawChar(labels[3], 106, 191, 20);
        graphics->drawChar(labels[5], 146, 191, 20);
        graphics->setColor(0xFFFFFF);
        graphics->drawChar(labels[1], 25, 190, 20);
        graphics->drawChar(labels[2], 65, 190, 20);
        graphics->drawChar(labels[3], 105, 190, 20);
        graphics->drawChar(labels[5], 145, 190, 20);
        this->drawIcon(graphics, 1, 13, 164);
        this->drawIcon(graphics, 2, 53, 164);
        this->drawIcon(graphics, 3, 93, 164);
        this->drawIcon(graphics, 5, 133, 164);
    } else if (n1 == 1) {
        graphics->drawChar(labels[0], 26, 191, 20);
        graphics->drawChar(labels[1], 66, 191, 20);
        graphics->drawChar(labels[2], 106, 191, 20);
        graphics->drawChar(labels[3], 146, 191, 20);
        graphics->setColor(0xFFFFFF);
        graphics->drawChar(labels[0], 25, 190, 20);
        graphics->drawChar(labels[1], 65, 190, 20);
        graphics->drawChar(labels[2], 105, 190, 20);
        graphics->drawChar(labels[3], 145, 190, 20);
        this->drawIcon(graphics, 0, 13, 164);
        this->drawIcon(graphics, 1, 53, 164);
        this->drawIcon(graphics, 2, 93, 164);
        this->drawIcon(graphics, 3, 133, 164);
    } else if (n1 == 2) {
        graphics->drawChar(labels[1], 26, 191, 20);
        graphics->drawChar(labels[2], 66, 191, 20);
        graphics->drawChar(labels[3], 106, 191, 20);
        graphics->drawChar(labels[4], 146, 191, 20);
        graphics->setColor(0xFFFFFF);
        graphics->drawChar(labels[1], 25, 190, 20);
        graphics->drawChar(labels[2], 65, 190, 20);
        graphics->drawChar(labels[3], 105, 190, 20);
        graphics->drawChar(labels[4], 145, 190, 20);
        this->drawIcon(graphics, 1, 13, 164);
        this->drawIcon(graphics, 2, 53, 164);
        this->drawIcon(graphics, 3, 93, 164);
        this->drawIcon(graphics, 4, 133, 164);
    }
    graphics->setClip(0, 0, canvas.getWidth(), canvas.getHeight());
}

void Extension::drawEffect(GameCanvas &canvas, Graphics *graphics, int32_t effect, int32_t x,
                           int32_t y) {
    (void)canvas;
    this->drawIcon(graphics, effect == 0 ? 6 : (effect == 1 ? 8 : 7), x, y);
}

Image *Extension::mapImage(Player &player) {
    if (mapImage_ == nullptr) {
        mapImage_ = Image::createImage(player.world_->platformContext(), (int32_t)89, (int32_t)89);
    }
    return mapImage_;
}

void Extension::drawMapFrame(GameCanvas &canvas, Graphics *graphics, int32_t n, int32_t n2,
                             int32_t n3, int32_t n4) {
    graphics->setColor(0);
    graphics->fillRect(0, 0, shiftLeft32(n3 * n4 + n, 1), shiftLeft32(n3 * n4 + n, 1));
    graphics->setColor(0xFFFFFF);
    graphics->drawRect(0, 0, shiftLeft32(n3 * n4 + n, 0), shiftLeft32(n3 * n4 + n, 0));
    if (n == 2) {
        graphics->drawRect(1, 1, shiftLeft32(n3 * n4 + n, -1), shiftLeft32(n3 * n4 + n, -1));
    }
    minimap::plot(graphics, canvas.mapGrid17_, n, n2, n3, n4);
}

void Extension::refreshMinimap(GameCanvas &canvas) {
    if (!canvas.mapDirty_) {
        return;
    }
    canvas.mapDirty_ = false;
    Player &player = *canvas.player_;
    int8_t by1 = player.gridX_;
    int8_t by2 = player.gridY_;
    int8_t by3 = player.facing_;
    if (canvas.mapMode_ == 1) {
        dungeonOf(player)->a(by1, by2, by3, 7, canvas.mapGrid17_);
        this->drawMapFrame(canvas, this->mapImage(player)->getGraphics(), 1, 1, 7, 3);
    } else {
        dungeonOf(player)->a(by1, by2, by3, 17, canvas.mapGrid17_);
        this->drawMapFrame(canvas, this->mapImage(player)->getGraphics(), 2, 2, 17, 5);
    }
}

void Extension::invalidateMinimap(GameCanvas &canvas) {
    canvas.mapDirty_ = true;
}

void Extension::drawMinimap(GameCanvas &canvas, Graphics *graphics) {
    Image *image = this->mapImage(*canvas.player_);
    if (canvas.mapMode_ == 1) {
        graphics->setClip(10, 20, 23, 23);
        graphics->drawImage(image, 10, 20, 20);
    } else {
        graphics->drawImage(image, 15, 25, 20);
    }
}

}
