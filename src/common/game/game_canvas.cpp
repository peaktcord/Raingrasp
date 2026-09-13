#include "src/common/game/game_canvas.hpp"

#include <exception>

#include "src/common/game/extension.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/spells.hpp"
#include "src/common/game/textwrap.hpp"
#include "src/common/game/util.hpp"

Font *GameCanvas::hudFont_ = nullptr;
Font *GameCanvas::bigFont_ = nullptr;
Font *GameCanvas::wideCompassFont_ = nullptr;
const int32_t GameCanvas::kWallLookup[5][6][4] = {
    {{12, 0, 0, 1}, {11, 0, -1, 1}, {12, 1, -1, 2}, {12, 2, -1, 3}, {11, 2, -2, 3}, {12, 3, -2, 4}},
    {{12, 0, 0, 1}, {12, 1, 0, 2}, {11, 1, -1, 2}, {12, 2, -1, 3}, {12, 3, -1, 4}, {12, 3, -1, 4}},
    {{12, 0, 0, 1}, {12, 1, 0, 2}, {12, 2, 0, 3}, {11, 2, -1, 3}, {12, 3, -1, 4}, {12, 3, -1, 4}},
    {{12, 0, 0, 1}, {12, 1, 0, 2}, {12, 2, 0, 3}, {12, 3, 0, 4}, {11, 3, -1, 4}, {11, 3, -1, 4}},
    {{12, 0, 0, 1}, {12, 1, 0, 2}, {12, 2, 0, 3}, {12, 3, 0, 4}, {12, 3, 0, 4}, {12, 3, 0, 4}}};
const char GameCanvas::kIconLabels[6] = {'F', 'C', 'R', 'O', 'T', 'Z'};
const char GameCanvas::kKeypadLabels[6] = {'1', '3', '5', '7', '9', '0'};

const char *GameCanvas::iconLabels() const {
    return game_->platformContext()->portOptions().letterKeyLabels ? kIconLabels
                                                                    : kKeypadLabels;
}
const char GameCanvas::kFacingChars[5] = {'0', 'N', 'E', 'S', 'W'};
const int32_t GameCanvas::kHudIconRows[3][3] = {{0, 0, 0}, {0, 1, 0}, {0, 2, 1}};
Image *GameCanvas::floorImage_ = nullptr;
Image *GameCanvas::floorIceImage_ = nullptr;
Image *GameCanvas::wallRightImage_ = nullptr;
Image *GameCanvas::wallInnerImage_ = nullptr;
Image *GameCanvas::gateImage_ = nullptr;
SharedArray<render::Sprite *> GameCanvas::npcSprites_;
SharedArray<render::Sprite *> GameCanvas::chestSprites_;
SharedArray<render::Sprite *> GameCanvas::bagSprites_;
SharedArray<render::Sprite *> GameCanvas::crystalSprites_;
SharedArray<Image *> GameCanvas::effectImages_;
Image *GameCanvas::hudIconAtlas_ = nullptr;
SharedArray<Image *> GameCanvas::hudIcons_;
Image *GameCanvas::hudPanelImage_ = nullptr;
SharedArray<std::string> GameCanvas::msgCannotCamp_;
SharedArray<std::string> GameCanvas::msgNoSpells_;
SharedArray<std::string> GameCanvas::msgNoMagicka_;
SharedArray<std::string> GameCanvas::msgNoMonster_;
SharedArray<std::string> GameCanvas::msgRestDisturbed_;
SharedArray<std::string> GameCanvas::msgRestComplete_;
SharedArray<std::string> GameCanvas::msgCreatureDead_;
SharedArray<std::string> GameCanvas::msgCreatureAttacks_;
SharedArray<std::string> GameCanvas::msgChest_;
SharedArray<std::string> GameCanvas::msgChestLocked_;
SharedArray<std::string> GameCanvas::msgInventoryFull_;
SharedArray<std::string> GameCanvas::msgFoundItem_;
SharedArray<std::string> GameCanvas::msgSeveralItems_;

void GameCanvas::initializeStatics(const game::Profile &profile) {
    hudFont_ = Font::getFont(64, 0, 8);
    bigFont_ = Font::getFont(64, 2, 16);
    wideCompassFont_ = Font::getFont(64, 1, 16);
    msgCannotCamp_ = SharedArray<std::string>{"Cannot", "Camp!"};
    msgNoSpells_ = SharedArray<std::string>{"No spells!", ""};
    msgNoMagicka_ = SharedArray<std::string>{"Not enough", profile.noMagickaWord};
    msgNoMonster_ = SharedArray<std::string>{"No monster", "here!"};
    msgRestDisturbed_ = SharedArray<std::string>{"Rest", "disturbed!"};
    msgRestComplete_ = SharedArray<std::string>{"Rest", "complete!"};
    msgCreatureDead_ = SharedArray<std::string>{"Creature", "is dead!"};
    msgCreatureAttacks_ = SharedArray<std::string>{"Creature", "attacks!"};
    msgChest_ = SharedArray<std::string>{"Chest", ""};
    msgChestLocked_ = SharedArray<std::string>{"Chest", "locked!"};
    msgInventoryFull_ = SharedArray<std::string>{"Inventory", "full!"};
    msgFoundItem_ = SharedArray<std::string>{"Found", "item!"};
    msgSeveralItems_ = SharedArray<std::string>{"Several", "items!"};
}

GameCanvas::GameCanvas(game::CanvasHost *game) : Canvas(game->platformContext()) {
    this->loopStarted_ = true;
    this->game_ = game;
    this->profile_ = &game->profile();
    this->prevGameAction_ = 0;
    this->gameAction_ = 0;
    this->running_ = false;
    this->paused_ = false;
    this->killRequested_ = false;
    this->player_ = nullptr;
    this->screenState_ = 1;
    this->stateChanged_ = false;
    this->campState_ = 0;
    this->diedAt_ = 0;
    this->campStartedAt_ = 0;
    this->pendingMove_ = 0;
    this->strafe_ = false;
    monsterAhead_ = false;
    chestAhead_ = false;
    npcAhead_ = -1;
    npcTileAhead_ = false;
    mapGrid7_ = makeSharedArray2D<int8_t>(7, 7);
    mapGrid17_ = makeSharedArray2D<int8_t>(17, 17);
    combatMonsterStorage_ = std::make_unique<Monster>();
    combatMonster_ = nullptr;
    hudLayout_ = 0;
    this->lastAttackMs_ = 0;
    this->lastCastMs_ = 0;
}

GameCanvas::~GameCanvas() = default;

game::Extension &GameCanvas::ext() { return player_->extension(); }
const game::Extension &GameCanvas::ext() const { return player_->extension(); }

const char *GameCanvas::viewLayerName(game::ViewLayer layer) {
    switch (layer) {
        case game::ViewLayer::Walls: return "Walls";
        case game::ViewLayer::Entities: return "Entities";
        case game::ViewLayer::Objects: return "Objects";
        case game::ViewLayer::Monsters: return "Monsters";
        case game::ViewLayer::NpcPortrait: return "NpcPortrait";
        case game::ViewLayer::Vitals: return "Vitals";
        case game::ViewLayer::IconRow: return "IconRow";
        case game::ViewLayer::MessageBox: return "MessageBox";
        case game::ViewLayer::Effects: return "Effects";
        case game::ViewLayer::ErrorText: return "ErrorText";
        case game::ViewLayer::Minimap: return "Minimap";
        case game::ViewLayer::End: return "End";
    }
    return "?";
}

// What was on screen when a frame failed?  paintBreadcrumb_ is updated as the
// frame is drawn but only reported if the paint throws, so a healthy run costs
// a couple of stores per layer and writes nothing to the log.
std::string GameCanvas::paintBreadcrumb() const {
    return std::string("screenState=") + std::to_string((int32_t)screenState_) +
           " campState=" + std::to_string((int32_t)campState_) + " layer=" +
           (painting_ ? viewLayerName(paintLayer_) : "(not painting)") +
           " npcAhead=" + std::to_string(npcAhead_) +
           " monsterAhead=" + (monsterAhead_ ? "1" : "0") + " dungeon=" +
           (player_ != nullptr ? std::to_string((int32_t)player_->dungeonId_)
                               : std::string("?")) +
           " sprites=" + std::to_string(residentSpriteCount()) + "/" +
           std::to_string(npcSprites_.length());
}

// The tick's handler catches far more than paint -- input dispatch, monster
// AI, the level-up screen, the minimap refresh.  Saying "while painting" for
// all of it sends the reader to the draw code for a failure that never got
// near it, so name what was actually running.
std::string GameCanvas::failureContext() const {
    return (painting_ ? std::string("while painting: ") : std::string("while ticking: ")) +
           this->paintBreadcrumb();
}

int32_t GameCanvas::residentSpriteCount() {
    int32_t resident = 0;
    for (int32_t i = 0; i < npcSprites_.length(); ++i) {
        if (npcSprites_[i] != nullptr) ++resident;
    }
    return resident;
}

void GameCanvas::paint(Graphics *graphics) {
    if (this->screenState_ == 3) {
        this->drawDeathScreen(graphics);
    } else if (this->campState_ == 1 || this->campState_ == 3 || this->campState_ == 2) {
        this->drawCampScreen(graphics);
    } else {
        this->drawGameView(graphics);
    }
}

void GameCanvas::drawDeathScreen(Graphics *graphics) {
    graphics->setColor(0);
    graphics->fillRect(0, 0, this->getWidth(), this->getHeight());
    graphics->setColor(0xFFFFFF);
    graphics->setFont(bigFont_);
    graphics->drawString("You're Dead!", this->getWidth() >> 1, this->getHeight() >> 1, 33);
}

void GameCanvas::drawCampScreen(Graphics *graphics) {
    graphics->setColor(0);
    graphics->fillRect(0, 0, this->getWidth(), this->getHeight());
    graphics->setColor(0xFFFFFF);
    graphics->setFont(bigFont_);
    graphics->drawString("CAMPING", this->getWidth() >> 1, this->getHeight() >> 1, 33);
}

// Marks a frame as in flight for the breadcrumb.  A frame that *completes*
// must not leave its last layer behind -- the tick's handler catches far more
// than paint, and a stale layer sends the reader to draw code for a failure
// that never went near it.  A frame that leaves by throwing is the opposite
// case: the handler reads the breadcrumb after unwinding, so the failing
// layer has to survive until it does.
class GameCanvas::PaintScope {
public:
    explicit PaintScope(GameCanvas *canvas) : canvas_(canvas) { canvas_->painting_ = true; }
    ~PaintScope() {
        if (std::uncaught_exceptions() > entryExceptions_) return;
        canvas_->painting_ = false;
        canvas_->paintLayer_ = game::ViewLayer::End;
    }
    PaintScope(const PaintScope &) = delete;
    PaintScope &operator=(const PaintScope &) = delete;

private:
    GameCanvas *canvas_;
    int entryExceptions_ = std::uncaught_exceptions();
};

void GameCanvas::drawGameView(Graphics *graphics) {
    PaintScope scope(this);
    graphics->setColor(0);
    graphics->fillRect(0, 0, this->getWidth(), this->getHeight());
    for (const game::ViewLayer *layer = profile_->viewLayers; *layer != game::ViewLayer::End;
         ++layer) {
        paintLayer_ = *layer;
        switch (*layer) {
            case game::ViewLayer::Walls:
                this->drawFloorAndWalls(graphics);
                break;
            case game::ViewLayer::Entities:
            case game::ViewLayer::Monsters:
                try {
                    this->drawSlots(graphics, *layer == game::ViewLayer::Entities, true);
                } catch (const std::exception &e) {
                    platform::writeLogLine(std::string("ERROR: failed to paint monsters: ") + e.what());
                    platform::writeLogLine("  " + this->paintBreadcrumb());
                } catch (...) {
                    platform::writeLogLine("ERROR: failed to paint monsters (unknown exception)");
                    platform::writeLogLine("  " + this->paintBreadcrumb());
                }
                break;
            case game::ViewLayer::Objects:
                this->drawSlots(graphics, true, false);
                break;
            case game::ViewLayer::NpcPortrait:
                if (npcAhead_ >= 0) {
                    ext().drawNpcPortrait(*this, graphics, npcAhead_);
                }
                break;
            case game::ViewLayer::Vitals:
                this->drawVitals(graphics);
                break;
            case game::ViewLayer::IconRow:
                this->drawIconRow(graphics);
                break;
            case game::ViewLayer::MessageBox:
                this->drawMessageBox(graphics);
                break;
            case game::ViewLayer::Effects:
                this->drawEffects(graphics);
                break;
            case game::ViewLayer::ErrorText:
                if (showError_) {
                    this->drawErrorText(graphics);
                }
                break;
            case game::ViewLayer::Minimap:
                this->drawCompassAndMap(graphics);
                break;
            case game::ViewLayer::End:
                break;
        }
    }
}

void GameCanvas::drawErrorText(Graphics *graphics) {
    graphics->setColor(0xFFFFFF);
    graphics->drawString(errorText_, 60, 10, 17);
}

void GameCanvas::drawEffects(Graphics *graphics) {
    int32_t n1;
    int32_t n2;
    GameRandom *random = game_->worldState().random;
    if (showBloodFx_) {
        n2 = 40 + GameUtil::randomInt(random, 30);
        n1 = 50 + GameUtil::randomInt(random, 20);
        ext().drawEffect(*this, graphics, 0, n2, n1);
        showBloodFx_ = false;
    }
    if (showMonsterSpellFx_) {
        n2 = 40 + GameUtil::randomInt(random, 30);
        n1 = 50 + GameUtil::randomInt(random, 22);
        ext().drawEffect(*this, graphics, 1, n2, n1);
        showMonsterSpellFx_ = false;
    }
    if (showSelfSpellFx_) {
        n2 = 50 + GameUtil::randomInt(random, 2);
        n1 = 80 + GameUtil::randomInt(random, 2);
        ext().drawEffect(*this, graphics, 2, n2, n1);
        showSelfSpellFx_ = false;
    }
    graphics->setClip(0, 0, this->getWidth(), this->getHeight());
}

void GameCanvas::drawFloorAndWalls(Graphics *graphics) {
    int32_t n1;
    int32_t n2;
    int32_t n3;
    int32_t n4;
    int32_t n5;
    int32_t n6;
    int32_t n7;
    int32_t n8;
    const int32_t dungeonId = this->player_->dungeon()->id_;
    if (!this->player_->hasAilment(3)) {
        if (this->player_->hasAilment(4)) {
            graphics->setColor(0xA00000);
            graphics->fillRect(0, 0, this->getWidth(), floorImage_->getHeight());
        } else {
            Image *floor = (profile_->iceFloorOutsideCamp && dungeonId != 1) ? floorIceImage_
                                                                              : floorImage_;
            n8 = 0;
            while (n8 < 5) {
                graphics->drawImage(floor, n8 * 36, 0, 20);
                ++n8;
            }
        }
    }
    ext().beginWalls(*this);
    n8 = 0;
    while (n8 < 5) {
        n7 = n8 * 18;
        n6 = 0;
        while (n6 < 6) {
            n5 = kWallLookup[n8][n6][0];
            n4 = kWallLookup[n8][n6][1];
            n3 = kWallLookup[n8][n6][2];
            n2 = kWallLookup[n8][n6][3];
            const int32_t kind = ext().wallKind(this->player_->peekAhead(n3, n2));
            if (kind != 0) {
                n1 = this->wallSlice(n5, n4, -1);
                ext().drawWallSlice(*this, graphics, n1, n7, kind);
                break;
            }
            ++n6;
        }
        ++n8;
    }
    n6 = 5;
    while (n6 < 10) {
        n7 = n6 * 18;
        n5 = 0;
        while (n5 < 6) {
            n4 = kWallLookup[9 - n6][n5][0];
            n3 = kWallLookup[9 - n6][n5][1];
            n2 = -kWallLookup[9 - n6][n5][2];
            int32_t n9 = kWallLookup[9 - n6][n5][3];
            const int32_t kind = ext().wallKind(this->player_->peekAhead(n2, n9));
            if (kind != 0) {
                n1 = this->wallSlice(n4, n3, 1);
                ext().drawWallSlice(*this, graphics, n1, n7, kind);
                break;
            }
            ++n5;
        }
        ++n6;
    }
    graphics->setClip(0, 0, this->getWidth(), this->getHeight());
}

int32_t GameCanvas::wallSlice(int32_t n, int32_t n2, int32_t n3) {
    if (n == 12) {
        return n2;
    }
    if (n3 == -1) {
        return 8 + n2;
    }
    return 7 - n2;
}

const game::SpriteBand *GameCanvas::spriteBandOf(int32_t type) const {
    for (int32_t n = 0; n < profile_->spriteBandCount; ++n) {
        const game::SpriteBand &band = profile_->spriteBands[n];
        if (type >= band.lo && type <= band.hi) {
            return &band;
        }
    }
    return nullptr;
}

void GameCanvas::drawSlots(Graphics *graphics, bool objects, bool monsters) {
    if (monsters) {
        campBlocked_ = false;
    }
    int32_t n1 = 8;
    while (n1 <= 12) {
        const visibility::Slot &e2 = player_->visibleSlots_[(size_t)n1];
        if (const SharedArray<int8_t> *record = e2.record()) {
            SharedArray<int8_t> object1 = *record;
            if (object1.length() == 28) {
                if (monsters && object1[6] != 0) {
                    campBlocked_ = true;
                    const game::SpriteBand *band = this->spriteBandOf((int32_t)object1[2]);
                    this->drawMonsterFar(graphics, band != nullptr ? band->farSprite : -1, n1);
                }
            } else if (object1.length() == 8 || object1.length() == 7) {
                if (objects) {
                    ext().drawObject(*this, graphics, object1, n1);
                }
            }
        } else if (const std::string *npc = e2.npc(); npc != nullptr && monsters) {
            ext().drawNpcSlot(*this, graphics, *npc, n1);
        }
        ++n1;
    }
    int32_t n2 = 4;
    while (n2 <= 6) {
        const visibility::Slot &object3 = player_->visibleSlots_[(size_t)n2];
        if (const SharedArray<int8_t> *record = object3.record()) {
            SharedArray<int8_t> object4 = *record;
            if (object4.length() == 28) {
                if (monsters && object4[6] != 0) {
                    campBlocked_ = true;
                    const game::SpriteBand *band = this->spriteBandOf((int32_t)object4[2]);
                    this->drawMonsterMid(graphics, band != nullptr ? band->midSprite : -1, n2);
                }
            } else if (object4.length() == 8 || object4.length() == 7) {
                if (objects) {
                    ext().drawObject(*this, graphics, object4, n2);
                }
            }
        } else if (const std::string *npc = object3.npc(); npc != nullptr && monsters) {
            ext().drawNpcSlot(*this, graphics, *npc, n2);
        }
        ++n2;
    }
    const visibility::Slot &object6 = player_->visibleSlots_[1];
    if (const SharedArray<int8_t> *record = object6.record()) {
        SharedArray<int8_t> object7 = *record;
        if (object7.length() == 28) {
            if (monsters && object7[6] != 0) {
                campBlocked_ = true;
                this->drawMonsterNear(graphics, (int32_t)object7[2], -1);
            }
        } else if (object7.length() == 8 || object7.length() == 7) {
            if (objects) {
                ext().drawObject(*this, graphics, object7, 1);
            }
        }
    }
}

void GameCanvas::drawMonsterNear(Graphics *graphics, int32_t n, int32_t n2) {
    if (ext().drawSpecialMonster(*this, graphics, n)) {
        graphics->setClip(0, 0, this->getWidth(), this->getHeight());
        return;
    }
    const game::SpriteBand *band = this->spriteBandOf(n);
    int32_t n1 = band != nullptr ? band->row : -1;
    if (n1 >= 0) {
        int8_t by1;
        int32_t n3;
        int32_t n4;
        const int8_t *layout = profile_->spriteLayout[n1];
        int8_t by2 = layout[2];
        int8_t by3 = layout[3];
        int8_t by4 = layout[4];
        int8_t by5 = layout[5];
        int32_t n5 = by2 + layout[6];
        int32_t n6 = by3 + layout[7];
        int8_t by6 = layout[8];
        int8_t by7 = layout[9];
        bool bl1 = by6 >= 0;
        int8_t by8 = profile_->spriteDrawMode[n - 1][0];
        int32_t n7 = profile_->spriteDrawMode[n - 1][1];
        if (n2 >= 0) {
            n7 = n2;
        }
        bool bl2 = profile_->spriteParts[n - 1][0] != 0;
        bool bl3 = profile_->spriteParts[n - 1][1] != 0;
        bool bl4 = profile_->spriteParts[n - 1][2] != 0;
        bool bl5 = profile_->spriteParts[n - 1][3] != 0;
        this->drawFrame(graphics, npcSprites_[by4], by8, by5, by2, by3);
        if (bl1) {
            this->drawFrame(graphics, npcSprites_[by6], n7, by7, n5, n6);
        }
        if (bl2) {
            n4 = by2 + layout[10];
            n3 = by3 + layout[11];
            by1 = layout[12];
            this->drawFrame(graphics, npcSprites_[by1], 0, 1, n4, n3);
        }
        if (bl3) {
            n4 = by2 + layout[13];
            n3 = by3 + layout[14];
            by1 = layout[15];
            this->drawFrame(graphics, npcSprites_[by1], 0, 1, n4, n3);
        }
        if (bl4) {
            n4 = by2 + layout[16];
            n3 = by3 + layout[17];
            by1 = layout[18];
            this->drawFrame(graphics, npcSprites_[by1], 0, 1, n4, n3);
        }
        if (bl5) {
            n4 = by2 + layout[19];
            n3 = by3 + layout[20];
            by1 = layout[21];
            this->drawFrame(graphics, npcSprites_[by1], 0, 1, n4, n3);
        }
    }
    graphics->setClip(0, 0, this->getWidth(), this->getHeight());
}

// Drawing the error screen is itself game code, and it runs while the tick has
// already failed.  If it throws too there is nothing useful left to do, so log
// it and let the caller report the failed tick rather than taking the process
// down from inside a catch block.
void GameCanvas::showErrorScreen() {
    try {
        this->repaint();
        this->serviceRepaints();
    } catch (const std::exception &nested) {
        platform::writeLogLine(std::string("ERROR: failed to draw the error screen: ") +
                               nested.what());
    } catch (...) {
        platform::writeLogLine("ERROR: failed to draw the error screen (unknown exception)");
    }
}

void GameCanvas::drawMonsterMid(Graphics *graphics, int32_t n, int32_t n2) {
    // -1 means the caller found no sprite band, and a resident slot is not
    // guaranteed between a dungeon change and the image loader finishing.
    if (n < 0 || n >= npcSprites_.length() || npcSprites_[n] == nullptr) {
        return;
    }
    int32_t n1 = 0;
    int32_t n3 = 38;
    switch (n2) {
        case 4: {
            n1 = 10;
            break;
        }
        case 5: {
            n1 = 62;
            break;
        }
        case 6: {
            n1 = 112;
        }
    }
    npcSprites_[n]->draw(graphics, n1, n3);
}

void GameCanvas::drawMonsterFar(Graphics *graphics, int32_t n, int32_t n2) {
    // drawSlots passes -1 when spriteBandOf finds no band for the monster.
    if (n < 0 || n >= npcSprites_.length() || npcSprites_[n] == nullptr) {
        return;
    }
    int32_t n1 = 0;
    int32_t n3 = 44;
    switch (n2) {
        case 8: {
            n1 = 10;
            break;
        }
        case 9: {
            n1 = 44;
            break;
        }
        case 10: {
            n1 = 79;
            break;
        }
        case 11: {
            n1 = 112;
            break;
        }
        case 12: {
            n1 = 146;
        }
    }
    npcSprites_[n]->draw(graphics, n1, n3);
}

void GameCanvas::drawFrame(Graphics *graphics, render::Sprite *sprite, int32_t n, int32_t n2,
                           int32_t n3, int32_t n4, int32_t manipulation) {
    // Monster art is unloaded wholesale on a dungeon change and reloaded only
    // for the bands that dungeon needs, and the loader can bail part way
    // through.  A slot that is not resident draws nothing; dereferencing it
    // was an access violation on whichever sprite happened to be missing.
    if (sprite == nullptr) {
        return;
    }
    int32_t n1 = sprite->width() / n2;
    int32_t n5 = sprite->height();
    graphics->setClip(n3, n4, n1, n5);
    sprite->draw(graphics, n3 - n * n1, n4, manipulation);
}

void GameCanvas::drawVitals(Graphics *graphics) {
    graphics->setColor(0xFFFF00);
    graphics->fillRect(5, 130, 40, 7);
    graphics->fillRect(5, 138, 40, 7);
    graphics->fillRect(5, 146, 40, 7);
    graphics->setColor(0xFF0000);
    int32_t n1 = this->player_->effectiveVital(2) * 38 / this->player_->vitals_[3];
    graphics->fillRect(6, 131, n1, 5);
    graphics->setColor(65280);
    n1 = this->player_->effectiveVital(4) * 38 / this->player_->vitals_[5];
    graphics->fillRect(6, 139, n1, 5);
    graphics->setColor(255);
    n1 = this->player_->effectiveVital(6) * 38 / this->player_->vitals_[7];
    if (n1 > 40) {
        n1 = 40;
    }
    graphics->fillRect(6, 147, n1, 5);
}

void GameCanvas::drawMessageBox(Graphics *graphics) {
    if (!messageVisible_) {
        return;
    }
    graphics->setColor(13080935);
    graphics->fillRoundRect(96, 118, 75, 35, 5, 5);
    graphics->setFont(hudFont_);
    graphics->setColor(0);
    graphics->drawString(messageLines_[0], 100, 122, 20);
    if (messageLines_.length() > 1) {
        graphics->drawString(messageLines_[1], 100, 134, 20);
    }
}

void GameCanvas::drawIconRow(Graphics *graphics) {
    hudLayout_ = this->hudLayoutFor();
    ext().drawIconRow(*this, graphics, hudLayout_);
}

int32_t GameCanvas::hudLayoutFor() {
    if (monsterAhead_) {
        return 1;
    }
    if (chestAhead_ || ext().talkLayoutForNpc(*this)) {
        return 2;
    }
    return 0;
}

void GameCanvas::drawCompassAndMap(Graphics *graphics) {
    if (this->player_->hasAilment(3)) {
        return;
    }
    graphics->setColor(0xFFFFFF);
    if (mapMode_ == 1) {
        graphics->setFont(hudFont_);
        graphics->drawChar(kFacingChars[this->player_->facing_], 16, 10, 20);
    } else {
        graphics->setFont(wideCompassFont_);
        graphics->drawChar(kFacingChars[this->player_->facing_], 58, 10, 20);
    }
    ext().drawMinimap(*this, graphics);
}

SharedArray<std::string> GameCanvas::messageLinesFor(const std::string &stringIn, LineRule rule) {
    std::string string1 = stringIn;
    if (profile_->hudLinesWrap) {
        SharedArray<std::string> stringArray1(2);
        std::vector<std::string> stringArray2 = textwrap::wrapSlack(hudFont_, 69, string1);
        if (stringArray2.size() == 1) {
            stringArray1[0] = std::string(stringArray2[0]);
            stringArray1[1] = "";
        } else {
            stringArray1[0] = stringArray2[0];
            stringArray1[1] = stringArray2[1];
        }
        return stringArray1;
    }
    switch (rule) {
        case LineRule::Whole:
            return SharedArray<std::string>{string1, std::string("")};
        case LineRule::FirstSpace: {
            SharedArray<std::string> stringArray1(2);
            size_t space = string1.find(' ');
            int32_t n1 = space == std::string::npos ? -1 : (int32_t)space;
            if (n1 < 0) {
                stringArray1[0] = string1;
                stringArray1[1] = std::string("");
            } else {
                stringArray1[0] = string1.substr(0, (size_t)n1);
                stringArray1[1] = string1.substr((size_t)n1 + 1);
            }
            return stringArray1;
        }
        case LineRule::Words:
        default: {
            SharedArray<std::string> stringArray1 = GameUtil::splitWhitespace(string1);
            SharedArray<std::string> stringArray2{std::string(""), std::string("")};
            if (stringArray1.length() >= 3) {
                stringArray2[0] = stringArray1[0] + " " + stringArray1[1];
                stringArray2[1] = stringArray1[2];
            } else {
                int32_t n3 = 0;
                while (n3 < stringArray1.length()) {
                    stringArray2[n3] = stringArray1[n3];
                    ++n3;
                }
            }
            return stringArray2;
        }
    }
}

bool GameCanvas::showMessage(SharedArray<std::string> stringArray, int32_t n) {
    if (n > messagePriority_ || n < 0) {
        messageLines_ = stringArray;
        messagePriority_ = n < 0 ? 10 : n;
        return true;
    }
    return false;
}

void GameCanvas::postMessage(SharedArray<std::string> lines, int32_t priority, int64_t nowMs) {
    if (this->showMessage(lines, priority)) {
        messageShownAt_ = nowMs;
        messageVisible_ = true;
    }
}

void GameCanvas::refreshNpcAhead() {
    int8_t by1 = this->player_->peekAhead(0, 1);
    npcTileAhead_ = GameUtil::hasFlag((int8_t)32, by1);
    if (npcTileAhead_) {
        int32_t n1 = this->player_->npcAhead();
        npcAhead_ = n1;
        if (n1 == -1) {
            platform::writeLogLine("ERROR: tile ahead is flagged as an NPC but no NPC is defined there");
        } else {
            this->postMessage(this->messageLinesFor(ext().npcName(n1), LineRule::Whole), 1,
                              game_->platformContext()->nowMillis());
        }
    } else {
        npcAhead_ = -1;
    }
    ext().onNpcAhead(*this, npcTileAhead_);
}

void GameCanvas::refreshMonsterAhead() {
    combatMonster_ = this->player_->monsterAhead(combatMonsterStorage_.get());
    if (combatMonster_ != nullptr) {
        monsterAhead_ = true;
        this->postMessage(this->messageLinesFor(combatMonster_->name(), LineRule::FirstSpace),
                          1, game_->platformContext()->nowMillis());
    } else {
        monsterAhead_ = false;
    }
}

void GameCanvas::refreshChestAhead() {
    SharedArray<int8_t> byArray1 = this->player_->chestAhead();
    chestAhead_ = !byArray1.isNull();
    if (chestAhead_) {
        this->postMessage(msgChest_, 1, game_->platformContext()->nowMillis());
    }
}

void GameCanvas::keyPressed(int32_t n) {
    this->prevGameAction_ = this->gameAction_;
    if (n == 49) {
        if (hudLayout_ == 1) {
            wantAttack_ = true;
        }
        return;
    }
    if (n == 50) {
        this->pendingMove_ = 1;
        return;
    }
    if (n == 51) {
        wantCast_ = true;
        return;
    }
    if (n == 52) {
        this->strafe_ = true;
        this->pendingMove_ = 4;
        return;
    }
    if (n == 53) {
        wantReadySpell_ = true;
        return;
    }
    if (n == 54) {
        this->strafe_ = true;
        this->pendingMove_ = 3;
        return;
    }
    if (n == 55) {
        wantOptions_ = true;
        return;
    }
    if (n == 56) {
        this->pendingMove_ = 2;
        return;
    }
    if (n == 57) {
        if (hudLayout_ == 2) {
            wantTalk_ = true;
        } else if (profile_->idleTalkKeyConsumesTick) {
            wantAction_ = true;
        }
        return;
    }
    if (n == 48) {
        if (hudLayout_ == 0) {
            wantCamp_ = true;
        }
        return;
    }
    if (n == 42) {
        if (++mapMode_ > 2) {
            mapMode_ = 1;
        }
        this->mapDirty_ = true;
        return;
    }
    this->strafe_ = false;
    this->gameAction_ = this->getGameAction(n);
    switch (this->gameAction_) {
        case 1: {
            this->pendingMove_ = 1;
            break;
        }
        case 6: {
            this->pendingMove_ = 2;
            break;
        }
        case 2: {
            this->pendingMove_ = 4;
            break;
        }
        case 5: {
            this->pendingMove_ = 3;
        }
    }
}

void GameCanvas::keyReleased(int32_t n) {
    int32_t n1 = this->getGameAction(n);
    (void)n1;
    this->prevGameAction_ = this->gameAction_;
    this->gameAction_ = 0;
}

void GameCanvas::stopLoop() {
    if (!this->loopStarted_) {
        return;
    }
    this->running_ = true;
    this->killRequested_ = true;
    this->loopStarted_ = false;
    if (profile_->loopStartSetsRunning) {
        this->running_ = false;
    }
}

void GameCanvas::startLoop() {
    try {
        this->stopLoop();
        if (profile_->loopStartSetsRunning) {
            this->running_ = true;
        }
        this->loopStarted_ = true;
        game_->startCanvasLoop(this);
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to start the game loop: ") +
                               exception.what());
        this->repaint();
        this->serviceRepaints();
    } catch (...) {
        platform::writeLogLine("ERROR: unhandled exception starting the game loop");
        this->repaint();
        this->serviceRepaints();
    }
}

int64_t GameCanvas::pacingDelayMs(TickStatus status) const {
    switch (status) {
        case TickStatus::Paused:
            return 250;
        case TickStatus::Ran: {
            int64_t elapsed = lastTickMs_;
            return elapsed < 250 ? 250 - elapsed : 0;
        }
        default:
            return 0;
    }
}

GameCanvas::TickStatus GameCanvas::tick() {
    if (!this->tickStateInitialised_) {
        this->tickStateInitialised_ = true;
        this->prevTickStartMs_ = this->tickStartMs_ =
            game_->platformContext()->nowMillis();
        this->tickDeltaMs_ = 0;
        this->perSecondAccumMs_ = 0;
    }

    int32_t n1;
    bool worldTicks;
    try {
        if (!this->running_) {
            return TickStatus::Stopped;
        }
        if (this->paused_) {
            if (!this->killRequested_) {
                return TickStatus::Paused;
            }
            this->killRequested_ = false;
            return TickStatus::Stopped;
        }

        worldTicks = true;
        actedThisTick_ = false;
        if (this->campState_ == 1 || this->campState_ == 3) {
            worldTicks = false;
            if (this->tickStartMs_ - this->campStartedAt_ > 2500) {
                if (this->campState_ == 3 || this->campAmbushRoll()) {
                    this->campStartedAt_ = 0;
                    this->stateChanged_ = true;
                    this->player_->rest(false);
                    ext().onCampAmbush(*this);
                    this->campState_ = 0;
                    worldTicks = true;
                    this->postMessage(msgRestDisturbed_, 1, this->tickStartMs_);
                } else {
                    this->campState_ = (int8_t)2;
                }
            }
        } else if (this->campState_ == 2) {
            worldTicks = false;
            if (this->tickStartMs_ - this->campStartedAt_ > 5000) {
                this->campState_ = 0;
                this->campStartedAt_ = 0;
                this->stateChanged_ = true;
                this->player_->rest(true);
                this->postMessage(msgRestComplete_, 1, this->tickStartMs_);
                worldTicks = true;
            }
        } else if (this->screenState_ != 1) {
            if (this->screenState_ == 2) {
                this->screenState_ = (int8_t)3;
                messageVisible_ = false;
                messagePriority_ = 0;
            }
            worldTicks = false;
            if (this->tickStartMs_ - this->diedAt_ > 5000) {
                platform::writeLogLine("Player: death recovery complete, resuming play");
                this->player_->restoreVitals(this->player_->vitals_);
                n1 = this->player_->itemCount_ - 1;
                while (n1 >= 0) {
                    if (!this->player_->isEquipped(n1)) {
                        this->player_->removeItem(n1);
                    }
                    --n1;
                }
                ext().onRespawn(*this);
                this->player_->resetForNewLife((int32_t)this->player_->classId_, true);
                this->diedAt_ = 0;
                this->screenState_ = 1;
                game_->worldState().npcs.wardenPending = true;
                this->stateChanged_ = true;
                this->mapDirty_ = true;
                announceDungeon_ = true;
                worldTicks = true;
                if (announceDungeon_) {
                    this->postMessage(ext().arrivalMessage(*this), 1,
                                      game_->platformContext()->nowMillis());
                    announceDungeon_ = false;
                }
            }
        }
        if (worldTicks) {
            ext().beforeWorldTick(*this);
            n1 = ext().runMonsters(*this, this->tickStartMs_);
            if ((n1 & 1) != 0) {
                this->mapDirty_ = true;
            }
            if ((n1 & 2) != 0) {
                game_->platformContext()->playSound(platform::Sound::PlayerHurt);
                this->postMessage(msgCreatureAttacks_, 2, this->tickStartMs_);
            }
            this->dispatchPendingAction(this->tickStartMs_);
            this->tickTimers(this->tickStartMs_, this->tickDeltaMs_);
            const bool levelled = profile_->promoteOnAward ? this->player_->leveledUp_
                                                           : this->player_->applyRankUps();
            if (levelled) {
                this->player_->leveledUp_ = false;
                this->pauseForUi();
                this->game_->showLevelUp();
                repaintEnabled_ = false;
            }
            this->player_->refreshVisible(false);
            ext().refreshMinimap(*this);
            this->stateChanged_ = false;
        }
        if (repaintEnabled_) {
            this->repaint();
            this->serviceRepaints();
        }
        lastTickMs_ = game_->platformContext()->nowMillis() - this->tickStartMs_;

        if (this->killRequested_) {
            this->killRequested_ = false;
            return TickStatus::Stopped;
        }
        this->prevTickStartMs_ = this->tickStartMs_;
        this->tickStartMs_ = game_->platformContext()->nowMillis();
        this->tickDeltaMs_ = this->tickStartMs_ - this->prevTickStartMs_;
        this->tickRegen(this->tickDeltaMs_);
        if ((this->perSecondAccumMs_ += this->tickDeltaMs_) > 1000) {
            this->perSecondAccumMs_ -= 1000;
            this->tickPerSecond();
        }
        if (this->tickStartMs_ - messageShownAt_ > 3000) {
            messageVisible_ = false;
            messagePriority_ = 0;
        }
        ext().afterTick(*this);
        return TickStatus::Ran;
    } catch (std::bad_alloc &) {
        platform::writeLogLine("ERROR: out of memory in the game loop");
        platform::writeLogLine("  " + this->failureContext());
        return TickStatus::Failed;
    } catch (std::exception &throwable) {
        platform::writeLogLine(std::string("ERROR: unhandled exception in the game loop: ") +
                               throwable.what());
        platform::writeLogLine("  " + this->failureContext());
        showError_ = true;
        errorText_ = std::string(throwable.what());
        this->showErrorScreen();
        return TickStatus::Failed;
    } catch (...) {
        platform::writeLogLine("ERROR: unhandled exception in the game loop (unknown exception)");
        platform::writeLogLine("  " + this->failureContext());
        showError_ = true;
        errorText_ = "(throwable)";
        this->showErrorScreen();
        return TickStatus::Failed;
    }
}

void GameCanvas::dispatchPendingAction(int64_t l) {
    if (wantCamp_) {
        if (campBlocked_) {
            this->postMessage(msgCannotCamp_, 1, l);
            wantCamp_ = false;
        } else {
            this->doCamp(l);
        }
    } else if (wantTalk_) {
        this->doTalk(l);
    } else if (wantCast_) {
        this->doCast(l);
    } else if (wantReadySpell_) {
        this->doReadySpell(l);
    } else if (wantAttack_) {
        this->doAttack(l);
    } else if (wantOptions_) {
        this->openOptions();
    } else if (wantAction_) {
        wantAction_ = false;
    } else if ((this->pendingMove_ != 0 || this->stateChanged_) && !this->stateChanged_) {
        this->doMove();
    }
    if (profile_->aheadRefreshEveryTick) {
        this->refreshChestAhead();
        this->refreshNpcAhead();
    }
    this->refreshMonsterAhead();
    this->handleMonsterDeath();
}

void GameCanvas::doAttack(int64_t l) {
    if (l - this->lastAttackMs_ >= 500 && combatMonster_ != nullptr) {
        actedThisTick_ = true;
        int8_t by1 = combatMonster_->hp_;
        this->player_->attackMonster(combatMonster_);
        this->lastAttackMs_ = l;
        const bool hit = by1 > combatMonster_->hp_;
        game_->platformContext()->playSound(hit ? platform::Sound::MeleeHit
                                                : platform::Sound::MeleeMiss);
        if (hit || profile_->bloodOnEverySwing) {
            showBloodFx_ = true;
        }
    }
    wantAttack_ = false;
}

void GameCanvas::handleMonsterDeath() {
    if (combatMonster_ != nullptr && combatMonster_->hp_ <= 0) {
        ext().onMonsterSlain(*this, *combatMonster_);
        this->player_->world_->removeMonsterAt((int32_t)this->player_->dungeonId_,
                                               (int32_t)combatMonster_->gridX_,
                                               (int32_t)combatMonster_->gridY_);
        if (this->player_->hasAilment(4)) {
            this->player_->vitals_[2] = (int16_t)(this->player_->vitals_[2] + 3 * this->player_->vitals_[3] / 10);
            this->player_->vitals_[2] = (int16_t)min32(this->player_->vitals_[2], this->player_->vitals_[3]);
        }
        game_->platformContext()->playSound(platform::Sound::MonsterDied);
        this->postMessage(msgCreatureDead_, 1, game_->platformContext()->nowMillis());
        combatMonster_ = nullptr;
        monsterAhead_ = false;
        this->mapDirty_ = true;
    }
}

void GameCanvas::doMove() {
    if (this->pendingMove_ != 0) {
        int8_t by1 = this->player_->itemCount_;
        actedThisTick_ = true;
        const bool stepped = this->player_->move(this->pendingMove_, this->strafe_);
        game_->platformContext()->playSound(stepped ? platform::Sound::PlayerStep
                                                    : platform::Sound::Blocked);
        if (this->player_->gameWon_) {
            platform::writeLogLine("Game: the player has won; showing the end of the game");
            this->game_->showEndOfGame();
            repaintEnabled_ = false;
        } else {
            if (ext().crossedBoundary(*this->player_) && (announceDungeon_ = true)) {
                this->postMessage(ext().arrivalMessage(*this), 1,
                                  game_->platformContext()->nowMillis());
                announceDungeon_ = false;
            }
            if (this->strafe_) {
                this->strafe_ = false;
            }
        }
        this->pendingMove_ = 0;
        int32_t n1 = this->player_->itemCount_ - by1;
        if (n1 >= 1) {
            game_->platformContext()->playSound(platform::Sound::ItemPickedUp);
        }
        if (n1 == 1) {
            this->postMessage(this->hudStrings(), -1,
                              game_->platformContext()->nowMillis());
        } else if (n1 > 1) {
            this->postMessage(msgSeveralItems_, -1,
                              game_->platformContext()->nowMillis());
        }
        if (!profile_->aheadRefreshEveryTick) {
            this->refreshChestAhead();
            this->refreshNpcAhead();
        }
        this->mapDirty_ = true;
    }
}

void GameCanvas::doCast(int64_t l) {
    if (wantCast_) {
        int8_t by1 = this->player_->readiedSpell_;
        if (!Spell::isValidId(by1)) {
            wantCast_ = false;
            return;
        }
        if (Spell::byId((int32_t)by1)->magickaCost_ > this->player_->effectiveVital(4)) {
            game_->platformContext()->playSound(platform::Sound::SpellFizzle);
            this->postMessage(msgNoMagicka_, 3, l);
        } else if (l - this->lastCastMs_ >= 500) {
            actedThisTick_ = true;
            if (Spell::targetsMonster(by1)) {
                if (!monsterAhead_) {
                    this->postMessage(msgNoMonster_, 1, l);
                } else {
                    this->player_->castAtMonster((int32_t)by1, combatMonster_);
                    game_->platformContext()->playSound(platform::Sound::SpellCast);
                    showMonsterSpellFx_ = true;
                }
            } else {
                this->player_->castSelf(by1);
                game_->platformContext()->playSound(platform::Sound::SpellCast);
                showSelfSpellFx_ = true;
            }
            this->lastCastMs_ = l;
        }
        wantCast_ = false;
    }
}

void GameCanvas::doReadySpell(int64_t l) {
    if (wantReadySpell_) {
        int32_t n1 = this->player_->nextSpell();
        if (n1 == 0) {
            this->postMessage(msgNoSpells_, -1, l);
        } else {
            this->player_->readiedSpell_ = (int8_t)n1;
            this->postMessage(this->messageLinesFor(Spell::byId(n1)->name_, LineRule::Words), -1,
                              l);
        }
        wantReadySpell_ = false;
    }
}

void GameCanvas::doTalk(int64_t l) {
    if (npcAhead_ >= 0) {
        // The matching "conversation over" is the screen log putting the player
        // back on the world view, so only the start needs naming here.
        platform::writeLogLine("NPC: started talking to " + ext().npcName(npcAhead_) + " (npc " +
                               std::to_string(npcAhead_) + ") in dungeon " +
                               std::to_string((int32_t)this->player_->dungeonId_));
        ext().openNpcScreen(*this, npcAhead_);
    } else if (chestAhead_) {
        SharedArray<int8_t> byArray1 = this->player_->chestAhead();
        int32_t n1 = this->player_->takeChest(byArray1);
        if (n1 == -1) {
            game_->platformContext()->playSound(platform::Sound::ChestLocked);
            this->postMessage(msgChestLocked_, 4, l);
        } else if (n1 == 0) {
            game_->platformContext()->playSound(platform::Sound::Blocked);
            this->postMessage(msgInventoryFull_, -1, l);
        } else {
            game_->platformContext()->playSound(platform::Sound::ChestOpened);
            chestAhead_ = false;
            this->mapDirty_ = true;
            this->postMessage(this->hudStrings(), -1, l);
        }
    }
    wantTalk_ = false;
}

SharedArray<std::string> GameCanvas::hudStrings() {
    int32_t n1 = this->player_->itemCount_ - 1;
    int32_t n2 = wrappingAbs(this->player_->inventory_[n1]);
    return this->messageLinesFor(Items::nameOf(n2), LineRule::Words);
}

void GameCanvas::openOptions() {
    this->game_->showOptions();
    repaintEnabled_ = false;
    wantOptions_ = false;
}

void GameCanvas::doCamp(int64_t l) {
    this->campState_ = 1;
    if (this->player_->potionEscape_) {
        this->campState_ = (int8_t)2;
    }
    this->campState_ = ext().campStateFor(*this, this->campState_);
    if (this->player_->dungeonId_ == 1) {
        this->campState_ = (int8_t)2;
    }
    this->campStartedAt_ = l;
    wantCamp_ = false;
}

void GameCanvas::tickTimers(int64_t l, int64_t l2) {
    int32_t n1 = this->player_->effectiveVital(2);
    if (n1 <= 0) {
        platform::writeLogLine("Player: died in dungeon " +
                               std::to_string((int32_t)this->player_->dungeonId_) + " at x=" +
                               std::to_string((int32_t)this->player_->gridX_) + " y=" +
                               std::to_string((int32_t)this->player_->gridY_));
        monsterAhead_ = false;
        this->screenState_ = (int8_t)2;
        this->diedAt_ = l;
        game_->platformContext()->playSound(platform::Sound::PlayerDied);
    }
    if (!actedThisTick_) {
        this->player_->regenFatigue(l2);
    }
}

void GameCanvas::pauseForUi() {
    this->paused_ = true;
}

void GameCanvas::resume() {
    this->paused_ = false;
}

void GameCanvas::showNotify() {
    this->player_->refreshSurroundings();
    ext().invalidateMinimap(*this);
    if (!profile_->aheadRefreshEveryTick) {
        this->refreshChestAhead();
    }
    this->refreshNpcAhead();
    this->refreshMonsterAhead();
    repaintEnabled_ = true;
    this->resume();
    if (announceDungeon_) {
        this->postMessage(ext().arrivalMessage(*this), 1,
                          game_->platformContext()->nowMillis());
        announceDungeon_ = false;
    }
}

bool GameCanvas::campAmbushRoll() {
    int32_t n1 = GameUtil::randomInt(game_->worldState().random, 10);
    return n1 == 1;
}

void GameCanvas::tickRegen(int64_t l) {
    int32_t n1;
    if (this->player_->hasAilment(4)) {
        this->player_->ailmentTimer4_ = (int16_t)((int64_t)this->player_->ailmentTimer4_ - l);
        if (this->player_->ailmentTimer4_ < 0) {
            this->player_->ailmentTimer4_ = 0;
            n1 = 3;
            this->player_->ailments_ = (int8_t)GameUtil::clearBit(n1, (int32_t)this->player_->ailments_);
        }
    }
    if (this->player_->hasAilment(5)) {
        this->player_->ailmentTimer5_ = (int16_t)((int64_t)this->player_->ailmentTimer5_ - l);
        if (this->player_->ailmentTimer5_ < 0) {
            this->player_->ailmentTimer5_ = 0;
            n1 = 4;
            this->player_->ailments_ = (int8_t)GameUtil::clearBit(n1, (int32_t)this->player_->ailments_);
        }
    }
    if (this->player_->hasAilment(7) && campBlocked_) {
        this->player_->ailmentTimer7_ = (int16_t)((int64_t)this->player_->ailmentTimer7_ - l);
        if (this->player_->ailmentTimer7_ < 0) {
            this->player_->ailmentTimer7_ = 0;
            n1 = 6;
            this->player_->ailments_ = (int8_t)GameUtil::clearBit(n1, (int32_t)this->player_->ailments_);
        }
    }
}

void GameCanvas::tickPerSecond() {
    int32_t n1;
    int32_t n2;
    if (this->player_->hasAilment(4)) {
        n2 = 2 * this->player_->vitals_[3] / 100;
        n2 = max32(n2, 0);
        this->player_->vitals_[2] = (int16_t)(this->player_->vitals_[2] - n2);
    }
    if (this->player_->hasAilment(5)) {
        n2 = this->player_->vitals_[5] / 10;
        this->player_->vitals_[4] = (int16_t)(this->player_->vitals_[4] + n2);
        if (this->player_->vitals_[4] >= this->player_->vitals_[5]) {
            this->player_->vitals_[4] = 0;
            n1 = this->player_->vitals_[5] / 10;
            this->player_->vitals_[2] = (int16_t)(this->player_->vitals_[2] - n1);
        }
    }
    n2 = 0;
    while (n2 < 25) {
        if (this->player_->spellTimers_[n2] > 0) {
            int32_t n3 = n2;
            this->player_->spellTimers_[n3] = (int8_t)(this->player_->spellTimers_[n3] - 1);
            if (this->player_->spellTimers_[n2] <= 0) {
                this->player_->spellTimers_[n2] = 0;
                if (n2 == 5 &&
                    (n1 = this->player_->findEquipped(profile_->conjuredWeaponItem)) != -1) {
                    this->player_->removeItem(n1);
                }
            }
        }
        ++n2;
    }
    const std::size_t dungeonIndex = (std::size_t)(this->player_->dungeonId_ - 1);
    if (game_->worldState().monsters.hasTable(dungeonIndex)) {
        Monster decodedMonster;
        for (const SharedArray<int8_t> &byArray1 : game_->worldState().monsters.at(dungeonIndex)) {
            Monster *d2 = Monster::fromRecord(&decodedMonster, byArray1, player_->dungeon());
            if (d2->effects_[6] == 0) continue;
            d2->effects_[7] = (int8_t)(d2->effects_[7] - 1);
            if (d2->effects_[7] >= 0) continue;
            d2->effects_[7] = 0;
            d2->effects_[6] = 0;
        }
    }
    ext().onSecond(*this);
}
