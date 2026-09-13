#include "src/common/game/player.hpp"

#include <stdexcept>

#include "src/common/game/formulas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/save_codec.hpp"
#include "src/common/game/smallhelpers.hpp"
#include "src/common/game/spells.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/visibility.hpp"
#include "src/common/game/vitals.hpp"

bool Player::dataLoaded_ = false;
int16_t Player::classCount_ = 0;
SharedArray<std::string> Player::classNames_;
SharedArray<std::string> Player::raceNames_;
SharedArray<std::string> Player::skillNames_;
SharedArray<SharedArray<int16_t>> Player::classTable_;
SharedArray<std::string> Player::vitalNames_;
SharedArray<std::string> Player::attributeNames_;
SharedArray<int16_t> Player::skillAttribute_;
const int32_t Player::startingKit_[7][2] = {{1, 27}, {7, 27}, {7, 22}, {17, 27},
                          {12, 22}, {17, 27}, {12, 22}};

void Player::initializeStatics() {
    dataLoaded_ = false;
}

int32_t Player::saveSize(bool bl) {
    return smallhelpers::saveSize(bl);
}

Player::Player(game::World *world) : Player(world, world->profile()) {}

Player::Player(game::World *world, const game::Profile &profile)
    : world_(world), profile_(&profile), extension_(profile.newExtension()) {
    gameWon_ = false;
    Player::ensureDataLoaded(world == nullptr ? nullptr : world->platformContext());
    this->name_ = std::string();
    this->vitals_ = SharedArray<int16_t>(10);
    this->attributes_ = SharedArray<int16_t>(16);
    this->vitalSeeds_ = SharedArray<int16_t>(2);
    this->skills_ = makeSharedArray2D<int16_t>(14, 3);
    this->itemCount_ = 0;
    this->inventory_ = SharedArray<int8_t>(24);
    this->itemData_ = SharedArray<int32_t>(24);
    this->equipped_ = SharedArray<int8_t>(7);
    this->spellTimers_ = SharedArray<int8_t>(25);
    this->surroundings_ = makeSharedArray2D<int8_t>(9, 5);
    this->warped_ = false;
}

Player::~Player() = default;

void Player::restoreVitals(SharedArray<int16_t> sArray) {
    GameFormulas::restoreVitals(sArray);
}

void Player::initFromClass(int32_t n) {
    int32_t n1;
    this->classId_ = (int16_t)n;
    this->raceId_ = classTable_[this->classId_][1];
    int32_t n2 = 8;
    int32_t n3 = 0;
    while (n3 < n2) {
        n1 = 2 * n3;
        this->attributes_[n1] = classTable_[this->classId_][2 + n3];
        this->attributes_[n1 + 1] = 0;
        ++n3;
    }
    this->luck_ = classTable_[this->classId_][10];
    this->vitalSeeds_[0] = classTable_[this->classId_][11];
    this->vitalSeeds_[1] = classTable_[this->classId_][12];
    this->vitals_[0] = 1;
    this->vitals_[1] = 0;
    this->recalcMaxVitals();
    this->vitals_[2] = this->vitals_[3];
    this->vitals_[4] = this->vitals_[5];
    this->vitals_[6] = this->vitals_[7];
    this->vitals_[8] = 0;
    this->vitals_[9] = 0;
    this->levelUpMask_ = 0;
    this->gold_ = profile_->startingGold;
    extension_->onInitFromClass(*this);
    n1 = 13;
    int32_t n4 = 0;
    while (n4 < 14) {
        this->skills_[n4][0] = classTable_[this->classId_][n1++];
        this->skills_[n4][1] = classTable_[this->classId_][n1++];
        this->skills_[n4][2] = 0;
        ++n4;
    }
    int32_t n5 = 0;
    while (n5 < 24) {
        this->inventory_[n5] = 0;
        this->itemData_[n5] = 0;
        ++n5;
    }
    int32_t n6 = 0;
    while (n6 < 7) {
        this->equipped_[n6] = 0;
        ++n6;
    }
    this->knownSpells_ = this->startingSpellMask();
}

void Player::recalcMaxVitals() {
    GameFormulas::calcMaxVitals(this->attributes_, this->luck_, this->vitals_[3], this->vitals_[5], this->vitals_[7]);
}

int32_t Player::startingSpellMask() {
    int32_t n1 = 0;
    int32_t n2 = 13;
    int32_t n3 = -1;
    bool bl1 = true;
    int32_t n4 = 0;
    while (n4 < 14) {
        int16_t s1 = classTable_[this->classId_][n2++];
        int16_t s2 = classTable_[this->classId_][n2++];
        (void)s2;
        switch (n4) {
            case 1: {
                n3 = 0;
                break;
            }
            case 3: {
                n3 = 5;
                break;
            }
            case 4: {
                n3 = 10;
                break;
            }
            case 6: {
                n3 = 15;
                break;
            }
            case 10: {
                n3 = 20;
                break;
            }
            default: {
                n3 = -1;
            }
        }
        if (n3 != -1 && s1 > 0) {
            n1 |= 1 << n3;
            if (bl1) {
                this->readiedSpell_ = (int8_t)(n3 + 1);
                bl1 = false;
            }
        }
        ++n4;
    }
    return n1;
}

void Player::resetForNewLife(int32_t n, bool bl) {
    (void)n;
    if (!bl) {
        this->giftPoints_ = 0;
        this->rumorsHeard_ = 0;
        extension_->onNewLife(*this);
    }
    this->ailments_ = 0;
    this->ailmentTimer4_ = 0;
    this->ailmentTimer5_ = 0;
    this->ailmentTimer7_ = 0;
    this->blessed_ = false;
    this->placeAtStart(bl);
    if (!bl) {
        this->recallDungeon_ = 0;
        this->recallX_ = 0;
        this->recallY_ = 0;
        this->recallFacing_ = 0;
    }
    int32_t n1 = 0;
    while (n1 < 25) {
        this->spellTimers_[n1] = 0;
        ++n1;
    }
    this->targetUid_ = 0;
    this->damageBonus_ = 0;
    this->potionAttack_ = false;
    this->potionDefence_ = false;
    this->potionEscape_ = false;
    if (!bl) {
        this->giveStartingKit();
    }
}

void Player::placeAtStart(bool bl) {
    extension_->onPlaceAtStart(*this);
    const game::StartPosition &at = bl ? profile_->campStart : profile_->newGameStart;
    this->nextDungeon_ = 1;
    this->dungeonId_ = 1;
    this->nextX_ = at.x;
    this->gridX_ = at.x;
    this->nextY_ = at.y;
    this->gridY_ = at.y;
    this->nextFacing_ = at.facing;
    this->facing_ = at.facing;
    this->refreshSurroundings();
    if (profile_->placementRefreshesCanvas) {
        world_->refreshAhead();
    }
}

std::string Player::classSummary() {
    std::string out;
    out.reserve(300);
    out += raceNames_[this->raceId_];
    out += ' ';
    out += classNames_[this->classId_];
    out += '\n';
    out += vitalNames_[0];
    out += ": ";
    out += std::to_string((int32_t)this->vitals_[0]);
    out += '\n';
    out += vitalNames_[2];
    out += ": ";
    out += std::to_string(this->effectiveVital(2));
    out += '\n';
    out += vitalNames_[4];
    out += ": ";
    out += std::to_string(this->effectiveVital(4));
    out += '\n';
    out += vitalNames_[6];
    out += ": ";
    out += std::to_string(this->effectiveVital(6));
    out += '\n';
    for (int32_t n2 = 0; n2 < 8; ++n2) {
        int32_t n1 = 2 * n2;
        out += attributeNames_[n1];
        out += ": ";
        out += std::to_string((int32_t)this->attributes_[n1]);
        out += '\n';
    }
    for (int32_t n1 = 0; n1 < 14; ++n1) {
        if (this->skills_[n1][0] > 0) {
            out += skillNames_[n1];
            out += ": ";
            out += std::to_string((int32_t)this->skills_[n1][0]);
            out += '\n';
        }
    }
    return std::string(out);
}

void Player::ensureDataLoaded(platform::PlatformContext *context) {
    if (!dataLoaded_) {
        try {
            Player::loadCharData(context, std::string("/charin.dat"));
            dataLoaded_ = true;
        } catch (const std::exception &exception) {
            platform::writeLogLine(std::string("ERROR: failed to load character data: ") +
                                   exception.what());
        }
    }
}

void Player::loadCharData(platform::PlatformContext *context, const std::string &string) {
    BinaryReader *dataInputStream = GameUtil::openResource(context, string);
    vitalNames_ = Player::readStringTable(dataInputStream);
    attributeNames_ = Player::readStringTable(dataInputStream);
    classNames_ = Player::readStringTable(dataInputStream);
    classCount_ = (int16_t)classNames_.length();
    raceNames_ = Player::readStringTable(dataInputStream);
    skillNames_ = Player::readStringTable(dataInputStream);
    int32_t n1 = skillNames_.length();
    if (n1 != 14) {
        throw std::runtime_error(
            "Error: mismatch between input number of skill types and that specified in code");
    }
    skillAttribute_ = SharedArray<int16_t>(n1);
    int32_t n2 = 0;
    while (n2 < n1) {
        Player::skillAttribute_[n2] = dataInputStream->readShort();
        ++n2;
    }
    int32_t n3 = 13 + 2 * n1;
    classTable_ = makeSharedArray2D<int16_t>(classCount_, n3);
    int32_t n4 = 0;
    while (n4 < classCount_) {
        int32_t n5 = 0;
        while (n5 < n3) {
            Player::classTable_[n4][n5] = dataInputStream->readShort();
            ++n5;
        }
        ++n4;
    }
    delete dataInputStream;
}

SharedArray<std::string> Player::readStringTable(BinaryReader *dataInputStream) {
    return smallhelpers::readStringTable(dataInputStream);
}

Player *Player::fromBytes(const game::Profile &profile, const SharedArray<int8_t> &byArray, bool bl) {
    return profile.saveCodec->fromBytes(byArray, bl);
}

SharedArray<int8_t> Player::toBytes(bool bl) {
    return profile_->saveCodec->toBytes(*this, bl);
}

void Player::stepCandidate(int32_t n) {
    int32_t n1 = -1;
    switch (n) {
        case 1: {
            n1 = 1;
        }
        case 2: {
            DungeonCore *object1;
            this->nextFacing_ = this->facing_;
            if (this->facing_ == 1) {
                this->nextX_ = this->gridX_;
                this->nextY_ = (int8_t)(this->gridY_ - n1);
            } else if (this->facing_ == 3) {
                this->nextX_ = this->gridX_;
                this->nextY_ = (int8_t)(this->gridY_ + n1);
            } else if (this->facing_ == 2) {
                this->nextX_ = (int8_t)(this->gridX_ + n1);
                this->nextY_ = this->gridY_;
            } else if (this->facing_ == 4) {
                this->nextX_ = (int8_t)(this->gridX_ - n1);
                this->nextY_ = this->gridY_;
            }
            DungeonCore *i2 = world_->dungeonAt(this->dungeonId_);
            if (this->nextX_ < 0) {
                this->crossedEdge_ = true;
                this->nextDungeon_ = i2->geometry_[3];
                object1 = world_->dungeonAt(this->nextDungeon_);
                if (this->nextDungeon_ == 1 || this->dungeonId_ == 1) {
                    this->nextX_ = (int8_t)(object1->width_ - 1);
                    this->nextY_ = (int8_t)(this->nextY_ + (object1->height_ - i2->height_) / 2);
                } else {
                    this->nextX_ = (int8_t)(object1->width_ - 1);
                }
            } else if (this->nextX_ >= i2->width_) {
                this->crossedEdge_ = true;
                this->nextDungeon_ = i2->geometry_[1];
                object1 = world_->dungeonAt(this->nextDungeon_);
                if (this->nextDungeon_ == 1 || this->dungeonId_ == 1) {
                    this->nextX_ = 0;
                    this->nextY_ = (int8_t)(this->nextY_ + (object1->height_ - i2->height_) / 2);
                } else {
                    this->nextX_ = 0;
                }
            } else if (this->nextY_ < 0) {
                this->crossedEdge_ = true;
                this->nextDungeon_ = i2->geometry_[0];
                object1 = world_->dungeonAt(this->nextDungeon_);
                if (this->nextDungeon_ == 1 || this->dungeonId_ == 1) {
                    this->nextX_ = (int8_t)(this->nextX_ + (object1->width_ - i2->width_) / 2);
                    this->nextY_ = (int8_t)(object1->height_ - 1);
                } else {
                    this->nextY_ = (int8_t)(object1->height_ - 1);
                }
            } else if (this->nextY_ >= i2->height_) {
                this->crossedEdge_ = true;
                this->nextDungeon_ = i2->geometry_[2];
                object1 = world_->dungeonAt(this->nextDungeon_);
                if (this->nextDungeon_ == 1 || this->dungeonId_ == 1) {
                    this->nextX_ = (int8_t)(this->nextX_ + (object1->width_ - i2->width_) / 2);
                    this->nextY_ = 0;
                } else {
                    this->nextY_ = 0;
                }
            } else {
                this->crossedEdge_ = false;
                this->nextDungeon_ = this->dungeonId_;
            }
            if (this->crossedEdge_) {
                extension_->onEdgeCandidate(*this);
            }
            break;
        }
        case 3: {
            this->nextDungeon_ = this->dungeonId_;
            this->crossedEdge_ = false;
            this->nextFacing_ = (int8_t)(this->facing_ + 1);
            if (this->nextFacing_ > 4) {
                this->nextFacing_ = 1;
            }
            this->nextX_ = this->gridX_;
            this->nextY_ = this->gridY_;
            break;
        }
        case 4: {
            this->nextDungeon_ = this->dungeonId_;
            this->crossedEdge_ = false;
            this->nextFacing_ = (int8_t)(this->facing_ - 1);
            if (this->nextFacing_ < 1) {
                this->nextFacing_ = (int8_t)4;
            }
            this->nextX_ = this->gridX_;
            this->nextY_ = this->gridY_;
        }
    }
}

bool Player::move(int32_t n, bool bl) {
    extension_->beforeMove(*this);
    if (this->vitals_[6] <= 0) {
        return false;
    }
    bool bl1 = false;
    bool bl2 = false;
    if (bl && n == 4) {
        this->tryStep(4);
        bl2 = this->tryStep(1);
        if (!(profile_->warpEndsStrafe && this->warped_)) {
            bl1 = this->crossedEdge_;
            bl2 = this->tryStep(3);
            this->crossedEdge_ = bl1;
        }
    } else if (bl && n == 3) {
        this->tryStep(3);
        bl2 = this->tryStep(1);
        if (!(profile_->warpEndsStrafe && this->warped_)) {
            bl1 = this->crossedEdge_;
            bl2 = this->tryStep(4);
            this->crossedEdge_ = bl1;
        }
    } else {
        bl2 = this->tryStep(n);
    }
    if (profile_->warpEndsStrafe) {
        this->warped_ = false;
    }
    return bl2;
}

bool Player::tryStep(int32_t n) {
    if (this->vitals_[6] <= 0) {
        return false;
    }
    if (n == 0) {
        return false;
    }
    this->stepCandidate(n);
    const bool forward = n == 1 || n == 2;
    if (this->nextDungeon_ <= 0) {
        return false;
    }
    DungeonCore *i2 = world_->dungeonAt(this->nextDungeon_);
    if (!i2->populated_) {
        return false;
    }
    int8_t by1 = i2->tiles_[this->nextX_][this->nextY_];
    if (!this->isWalkable(by1)) {
        return false;
    }
    extension_->beforeStep(*this);
    if (this->nextDungeon_ != this->dungeonId_) {
        platform::writeLogLine("Map: dungeon " + std::to_string((int32_t)this->dungeonId_) +
                               " -> " + std::to_string((int32_t)this->nextDungeon_) +
                               " at x=" + std::to_string((int32_t)this->nextX_) +
                               " y=" + std::to_string((int32_t)this->nextY_));
    }
    this->dungeonId_ = this->nextDungeon_;
    this->prevX_ = this->gridX_;
    this->prevY_ = this->gridY_;
    this->gridX_ = this->nextX_;
    this->gridY_ = this->nextY_;
    this->facing_ = this->nextFacing_;
    i2->visited_ = true;
    if (forward) {
        worldstate::WorldState &worldState = world_->worldState();
        if (worldState.npcs.wardenPending) {
            worldState.npcs.wardenPending = false;
        }
        this->vitals_[6] = (int16_t)(this->vitals_[6] - 1 * this->fatigueMultiplier());
        this->vitals_[6] = (int16_t)max32(this->vitals_[6], 0);
    }
    if ((by1 & 4) != 0) {
        if (extension_->takeItemsUnderfoot(*this, by1, forward)) {
            this->refreshSurroundings();
            return true;
        }
    }
    this->refreshSurroundings();
    if (forward && (by1 & 8) != 0) {
        this->warpToCamp(false);
        if (!profile_->warpEndsStrafe) {
            this->warped_ = false;
        }
    }
    return true;
}

bool Player::isWalkable(int8_t by) {
    return smallhelpers::isWalkable(by);
}

melee::Attacker Player::attacker() {
    melee::Attacker a;
    a.random = world_->worldState().random;
    a.self = this;
    a.vitals = this->vitals_;
    a.attackSkill = [](void *self, bool withAttribute) {
        return static_cast<Player *>(self)->attackSkill(withAttribute);
    };
    a.attackAptitude = [](void *self) { return static_cast<Player *>(self)->attackAptitude(); };
    a.attackSkillIndex = [](void *self) { return static_cast<Player *>(self)->attackSkillIndex(); };
    a.weaponDamage = [](void *self) { return static_cast<Player *>(self)->weaponDamage(); };
    a.fatigueMultiplier = [](void *self) {
        return static_cast<Player *>(self)->fatigueMultiplier();
    };
    a.spellActive = [](void *self, int32_t spell) {
        return static_cast<Player *>(self)->spellActive(spell);
    };
    a.clearSpell = [](void *self, int32_t spell) {
        static_cast<Player *>(self)->clearSpell(spell);
    };
    a.hasAilment = [](void *self, int32_t ailment) {
        return static_cast<Player *>(self)->hasAilment(ailment);
    };
    a.awardSkillXp = [](void *self, int32_t skill, int32_t points) {
        static_cast<Player *>(self)->awardSkillXp(skill, points);
    };
    a.setTargetUid = [](void *self, int16_t uid) {
        static_cast<Player *>(self)->targetUid_ = uid;
    };
    a.clearSpellTimerRaw = [](void *self, int32_t index) {
        static_cast<Player *>(self)->spellTimers_[index] = 0;
    };
    return a;
}

spellcast::Caster Player::caster() {
    spellcast::Caster c;
    c.random = world_->worldState().random;
    c.self = this;
    c.vitals = this->vitals_;
    c.fatigueMultiplier = [](void *self) {
        return static_cast<Player *>(self)->fatigueMultiplier();
    };
    c.hasAilment = [](void *self, int32_t ailment) {
        return static_cast<Player *>(self)->hasAilment(ailment);
    };
    c.awardSkillXp = [](void *self, int32_t skill, int32_t points) {
        static_cast<Player *>(self)->awardSkillXp(skill, points);
    };
    return c;
}

namespace {

melee::Target meleeTarget(Monster *m) {
    melee::Target t;
    t.self = m;
    t.uid = m->uid_;
    t.type = m->type_;
    t.stat = [](void *self, int32_t which) { return static_cast<Monster *>(self)->stat(which); };
    t.effect = [](void *self, int32_t index) {
        return static_cast<Monster *>(self)->effects_[index];
    };
    t.takeDamage = [](void *self, int32_t amount) {
        static_cast<Monster *>(self)->takeDamage(amount);
    };
    t.store = [](void *self) { static_cast<Monster *>(self)->store(); };
    return t;
}

struct SlotRule {
    int32_t column;
    int32_t depth;
    int32_t slot;
    int32_t betweenCount;
    int32_t between[5];
};

const SlotRule kSlotRules[] = {
    {3, 2, 1, 0, {}},
    {2, 1, 4, 3, {0, 1, 5}},
    {3, 1, 5, 1, {1}},
    {4, 1, 6, 3, {1, 2, 5}},
    {1, 0, 8, 5, {0, 1, 3, 4, 9}},
    {2, 0, 9, 5, {0, 1, 4, 5, 10}},
    {3, 0, 10, 2, {1, 5}},
    {4, 0, 11, 5, {1, 2, 5, 6, 10}},
    {5, 0, 12, 5, {1, 2, 6, 7, 11}},
};

}

void Player::attackMonster(Monster *d2) {
    melee::swing(this->attacker(), meleeTarget(d2));
}

void Player::markSeen(int32_t n, const visibility::Slot &object) {
    SharedArray<int8_t> byArray1;
    switch (n) {
        case 1: {
            byArray1 = *object.record();
            byArray1[6] = 1;
            Monster monster;
            Monster *d2 = Monster::fromRecord(&monster, byArray1, dungeon());
            std::string string1 = std::to_string((int32_t)d2->uid_);
            (void)string1;
            d2->seen_ = true;
            d2->store();
            break;
        }
        case 2: {
            byArray1 = *object.record();
            byArray1[6] = (int8_t)(byArray1[6] | 1);
        }
    }
}

void Player::refreshVisible(bool bl) {
    (void)bl;
    this->resetVisible();
    for (int32_t layer = 0; layer < 3; ++layer) {
        this->placeVisibleLayer(profile_->visibleLayerOrder[layer]);
    }
    extension_->placeVisibleNpcs(*this);
}

void Player::placeVisibleLayer(int32_t kind) {
    worldstate::WorldState &worldState = world_->worldState();
    const std::size_t dungeonIndex = (std::size_t)(this->dungeonId_ - 1);
    if (kind == 1) {
        if (worldState.monsters.hasTable(dungeonIndex)) {
            const worldstate::MonsterList records = worldState.monsters.at(dungeonIndex);
            for (const SharedArray<int8_t> &record : records) {
                visibility::Slot object(record);
                if (!this->placeVisible(1, object)) continue;
                this->markSeen(1, object);
            }
        }
    } else if (kind == 4) {
        if (worldState.chests.hasTable(dungeonIndex)) {
            for (const SharedArray<int8_t> &record : worldState.chests.at(dungeonIndex)) {
                visibility::Slot object(record);
                this->placeVisible(4, object);
            }
        }
    } else if (kind == 2) {
        const worldstate::DroppedItemList &dropped = worldState.droppedItems.at(dungeonIndex);
        for (const SharedArray<int8_t> &record : dropped) {
            visibility::Slot object(record);
            if (!this->placeVisible(2, object)) continue;
            this->markSeen(2, object);
        }
    }
}

int8_t Player::peekAhead(int32_t n, int32_t n2) {
    if (n2 < 4) {
        return this->surroundings_[n + n2 + 1][n2];
    }
    return this->surroundings_[n + n2][n2];
}

void Player::resetVisible() {
    visibility::View v;
    v.self = this;
    v.slots = &visibleSlots_;
    v.peek = [](void *self, int32_t column, int32_t depth) {
        return static_cast<Player *>(self)->peekAhead(column, depth);
    };
    visibility::reset(v);
}

bool Player::placeVisible(int32_t n, const visibility::Slot &object) {
    int32_t n1;
    int32_t n2;
    int8_t by1 = 0;
    int8_t by2 = 0;
    if (n == 1) {
        SharedArray<int8_t> byArray1 = *object.record();
        by1 = byArray1[4];
        by2 = byArray1[5];
    } else if (n == 4) {
        SharedArray<int8_t> byArray2 = *object.record();
        by1 = byArray2[0];
        by2 = byArray2[1];
    } else if (extension_->npcPosition(*this, n, object, by1, by2)) {
    } else {
        SharedArray<int8_t> byArray3 = *object.record();
        by1 = byArray3[0];
        by2 = byArray3[1];
    }
    int32_t n3 = 0;
    n2 = 0;
    if (this->facing_ == 1 || this->facing_ == 3) {
        n1 = -1;
        if (this->facing_ == 1) {
            n1 = 1;
        }
        n3 = n1 * (by1 - this->gridX_) + 3;
        n2 = n1 * (by2 - this->gridY_) + 3;
    } else if (this->facing_ == 2 || this->facing_ == 4) {
        n1 = -1;
        if (this->facing_ == 2) {
            n1 = 1;
        }
        n3 = n1 * (by2 - this->gridY_) + 3;
        n2 = 3 - n1 * (by1 - this->gridX_);
    }
    for (const SlotRule &rule : kSlotRules) {
        if (n3 != rule.column || n2 != rule.depth) continue;
        bool clear = true;
        if (profile_->placementGuard == game::PlacementGuard::DestinationEmpty) {
            clear = visibleSlots_[(size_t)rule.slot].is(visibility::SlotState::Empty);
        } else {
            for (int32_t k = 0; k < rule.betweenCount; ++k) {
                if (Player::blocksView(visibleSlots_[(size_t)rule.between[k]])) {
                    clear = false;
                    break;
                }
            }
        }
        if (!clear) {
            return false;
        }
        visibleSlots_[(size_t)rule.slot] = object;
        return true;
    }
    return false;
}

bool Player::blocksView(const visibility::Slot &object) {
    return visibility::blocks(object);
}

int32_t Player::combatRoll(int32_t n, int32_t n2) {
    return GameFormulas::resolveCombatRoll(world_->worldState().random, n, n2);
}

int32_t Player::skillRank(int32_t n, bool bl) {
    int32_t n1 = this->skills_[n][0];
    if (bl) {
        int32_t n2 = 1 + skillAttribute_[n];
        n1 += this->attributes_[n2] / 3;
    }
    if (n == skills::SECURITY && this->spellActive(spellid::DEFT_SECURITY)) {
        n1 += this->skills_[skills::ALTERATION][0];
    }
    if (this->vitals_[6] < 7) {
        --n1;
    }
    n1 += extension_->skillRankBonus(*this);
    return n1;
}

int32_t Player::skillAptitude(int32_t n) {
    int16_t s1 = this->skills_[n][1];
    return s1;
}

combatstats::Combatant Player::combatant() const {
    combatstats::Combatant c;
    c.equipped = &this->equipped_[0];
    c.self = this;
    c.rank = [](const void *self, int32_t skill, bool withAttribute) {
        return const_cast<Player *>(static_cast<const Player *>(self))->skillRank(skill, withAttribute);
    };
    c.aptitude = [](const void *self, int32_t skill) {
        return const_cast<Player *>(static_cast<const Player *>(self))->skillAptitude(skill);
    };
    c.effect = [](const void *self, int32_t effect) {
        return const_cast<Player *>(static_cast<const Player *>(self))->spellActive(effect);
    };
    c.damageBonus = this->damageBonus_;
    c.potionAttack = this->potionAttack_;
    c.potionDefence = this->potionDefence_;
    return c;
}

int32_t Player::defenceSkill(bool bl) { return combatstats::defenceSkill(combatant(), bl); }

int32_t Player::defenceAptitude() { return combatstats::defenceAptitude(combatant()); }

int32_t Player::bestWeaponSkill() { return combatstats::bestWeaponSkill(combatant()); }

int32_t Player::attackSkillIndex() { return combatstats::attackSkillIndex(combatant()); }

int32_t Player::attackSkill(bool bl) { return combatstats::attackSkill(combatant(), bl); }

int32_t Player::attackAptitude() { return combatstats::attackAptitude(combatant()); }

int32_t Player::weaponDamage() { return combatstats::weaponDamage(combatant()); }

int32_t Player::defenceSkillIndex() { return combatstats::defenceSkillIndex(combatant()); }

int32_t Player::armourRating() { return combatstats::armourRating(combatant()); }

int32_t Player::takeChest(const SharedArray<int8_t> &byArray) {
    byArray[2] = 2;
    if (this->hasRoom()) {
        int8_t by1 = byArray[4];
        int32_t n1 = (byArray[5] << 8) + byArray[6];
        int8_t by2 = byArray[7];
        this->addItem(by1, n1, by2);
        this->dungeon()->removeChest(byArray);
        int32_t n2 = by1 - 1;
        if (Items::at(n2).category == 11) {
            this->giftPoints_ = (int16_t)(this->giftPoints_ + (int16_t)Items::at(n2).tier);
            extension_->onGiftPoints(*this);
        }
        return 1;
    }
    SharedArray<int8_t> byArray1{byArray[0], byArray[1], byArray[4],
                           byArray[5], byArray[6], byArray[7], 1};
    this->dungeon()->addDroppedItem(byArray1);
    this->dungeon()->removeChest(byArray);
    return 0;
}

bool Player::spellActive(int32_t n) {
    return vitals::spellActive(this->spellTimers_, n, this->targetUid_ != 0);
}

void Player::clearSpell(int32_t n) {
    vitals::clearSpell(this->spellTimers_, n);
}

Monster *Player::monsterAhead(Monster *target) {
    this->stepCandidate(1);
    if (this->nextDungeon_ <= 0) {
        return nullptr;
    }
    if (target == nullptr) {
        target = new Monster();
    }
    return world_->dungeonAt(this->nextDungeon_)->monsterAt(target, this->nextX_, this->nextY_);
}

SharedArray<int8_t> Player::chestAhead() {
    this->stepCandidate(1);
    if (this->nextDungeon_ <= 0) {
        return SharedArray<int8_t>();
    }
    int8_t by1 = this->nextDungeon_;
    return world_->worldState().chests.findAt((std::size_t)(by1 - 1), this->nextX_,
                                              this->nextY_);
}

int32_t Player::npcAhead() {
    this->stepCandidate(1);
    if (this->nextDungeon_ <= 0) {
        return -1;
    }
    return extension_->npcAhead(*this, this->nextDungeon_, this->nextX_, this->nextY_);
}

int32_t Player::spellSchool(int32_t n) {
    return vitals::spellSchool(n);
}

void Player::castSelf(int32_t n) {
    int32_t school = this->spellSchool(n);
    Spell *spell = Spell::byId(n);
    int32_t gap = this->skillRank(school, true) - spell->level_;
    int8_t by4 = spell->effect_;
    spellcast::Outcome out = spellcast::resolve(
        this->caster(), school, this->skillAptitude(school) + gap * 5,
        spell->difficulty_ - gap * 5, spell->magickaCost_);
    switch (n) {
        case spellid::FRENZY:
        case spellid::SHIELD:
        case spellid::DEFT_SECURITY:
        case spellid::DRAGON_COMBAT: {
            this->spellTimers_[n - 1] = (int8_t)(by4 * out.multiplier);
            break;
        }
        case spellid::DAEDRIC_WEAPON: {
            int32_t n9 = 0;
            if (!this->addItem(profile_->conjuredWeaponItem, n9, 0) || !this->equipLast(true)) break;
            this->spellTimers_[n - 1] = (int8_t)(by4 * out.multiplier);
            break;
        }
        case spellid::RAISE_ATTRIBUTE: {
            this->spellTimers_[n - 1] = -2;
            break;
        }
        case spellid::HEAL_WOUND: {
            int32_t n10 = 6 + this->skillRank(skills::RESTORATION, false);
            this->vitals_[2] = (int16_t)(this->vitals_[2] + out.multiplier * n10);
            this->vitals_[2] = (int16_t)min32(this->vitals_[2], this->vitals_[3]);
            break;
        }
        case spellid::CAMP_MAGICKA: {
            this->spellTimers_[n - 1] = -4;
            break;
        }
        case spellid::REMOVE_AILMENT: {
            int32_t n11 = 1;
            while (n11 <= out.multiplier) {
                this->cureOneAilment();
                ++n11;
            }
            break;
        }
    }
    spellcast::settle(this->caster());
}

void Player::castAtMonster(int32_t n, Monster *d2) {
    spellcast::Target t;
    t.self = d2;
    t.stat = [](void *self, int32_t which) { return static_cast<Monster *>(self)->stat(which); };
    t.setEffect = [](void *self, int32_t index, int8_t value) {
        static_cast<Monster *>(self)->effects_[index] = value;
    };
    t.takeDamage = [](void *self, int32_t amount) {
        static_cast<Monster *>(self)->takeDamage(amount);
    };
    t.isUndead = [](void *self) { return static_cast<Monster *>(self)->isUndead(); };
    t.store = [](void *self) { static_cast<Monster *>(self)->store(); };

    spellcast::MonsterSpellHooks h;
    h.monster = d2;
    h.skillRank = [](void *self, int32_t skill) {
        return static_cast<Player *>(self)->skillRank(skill, false);
    };
    h.skillRankWithAttribute = [](void *self, int32_t skill) {
        return static_cast<Player *>(self)->skillRank(skill, true);
    };
    h.skillAptitude = [](void *self, int32_t skill) {
        return static_cast<Player *>(self)->skillAptitude(skill);
    };
    h.setSpellTimer = [](void *self, int32_t index, int8_t value) {
        static_cast<Player *>(self)->spellTimers_[index] = value;
    };
    h.setDamageBonus = [](void *self, int16_t bonus) {
        static_cast<Player *>(self)->damageBonus_ = bonus;
    };
    h.attackMonster = [](void *self, void *monster) {
        static_cast<Player *>(self)->attackMonster(static_cast<Monster *>(monster));
    };

    spellcast::castAtMonster(this->caster(), t, h, n, this->spellSchool(n),
                             Spell::byId(n)->magickaCost_);
}

progression::Progress Player::progress() {
    progression::Progress p;
    p.skills = this->skills_;
    p.skillAttribute = skillAttribute_;
    p.vitals = this->vitals_;
    p.levelUpMask = &this->levelUpMask_;
    return p;
}

progression::Rules Player::progressionRules() {
    return profile_->progressionRules;
}

void Player::awardSkillXp(int32_t n, int32_t n2) {
    progression::award(this->progress(), n, n2);
    if (!profile_->promoteOnAward) {
        return;
    }
    if (n < 0 || n >= progression::kSkillCount) {
        return;
    }
    if (progression::sweep(this->progress(), progressionRules(), n, n + 1)) {
        this->leveledUp_ = true;
    }
}

bool Player::applyRankUps() {
    return progression::sweepAll(this->progress(), progressionRules());
}

bool Player::takeDroppedItem(const SharedArray<int8_t> &byArray) {
    int8_t by1 = byArray[2];
    int32_t n1 = (byArray[3] << 8) + byArray[4];
    int8_t by2 = byArray[5];
    return this->addItem(by1, n1, by2);
}

bool Player::addItem(int32_t n, int32_t n2, int32_t n3) {
    inventory::Bag bag = this->bag();
    return inventory::addItem(bag, n, n2, n3);
}

bool Player::removeItem(int32_t n) {
    inventory::Bag bag = this->bag();
    return inventory::removeItem(bag, n);
}

void Player::dropItem(int32_t n) {
    int32_t n1 = wrappingAbs(this->inventory_[n]);
    if (n1 != profile_->conjuredWeaponItem) {
        SharedArray<int8_t> byArray1(7);
        byArray1[0] = this->gridX_;
        byArray1[1] = this->gridY_;
        byArray1[2] = (int8_t)n1;
        byArray1[5] = (int8_t)(this->itemData_[n] & 0xFF);
        int32_t n2 = unsignedShiftRight32(this->itemData_[n], 16) & 0xFFFF;
        byArray1[3] = (int8_t)((n2 >> 8) & 0xFF);
        byArray1[4] = (int8_t)(n2 & 0xFF);
        byArray1[6] = 3;
        this->dungeon()->addDroppedItem(byArray1);
        this->removeItem(n);
    } else {
        this->removeItem(n);
    }
}

bool Player::isEquipped(int32_t n) {
    return inventory::isEquipped(this->inventory_, n);
}

bool Player::equip(int32_t n, bool bl) {
    inventory::Bag bag = this->bag();
    return inventory::equip(bag, n, bl);
}

void Player::unequipSlot(int32_t n) {
    inventory::Bag bag = this->bag();
    inventory::unequipSlot(bag, n);
}

void Player::addGold(int32_t n) {
    this->gold_ += n;
}

bool Player::equipLast(bool bl) {
    return this->equip(this->itemCount_ - 1, bl);
}

void Player::unequipItem(int32_t n) {
    inventory::Bag bag = this->bag();
    inventory::unequipItem(bag, n);
}

int32_t Player::findEquipped(int32_t n) {
    return inventory::findEquipped(this->inventory_, this->itemCount_, n);
}

inventory::Bag Player::bag() {
    inventory::Bag b;
    b.items = &this->inventory_;
    b.data = &this->itemData_;
    b.equipped = &this->equipped_;
    b.count = &this->itemCount_;
    return b;
}

bool Player::hasRoom() {
    return inventory::hasRoom(this->itemCount_);
}

std::string Player::describeItem(int32_t n) {
    int32_t n1 = wrappingAbs(this->inventory_[n]);
    int8_t by1 = Items::at(n1 - 1).category;
    std::string string1;
    switch (by1) {
        case 1:
        case 2:
        case 3:
        case 4: {
            string1 = Items::at(n1 - 1).name + '\n' + Items::categoryNames_[by1 - 1];
            int32_t n2 = Items::at(n1 - 1).rating + (this->itemData_[n] & 0xFF);
            string1 += "\nWeapon value: " + std::to_string(n2);
            break;
        }
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10: {
            string1 = Items::at(n1 - 1).name + '\n' + Items::categoryNames_[by1 - 1];
            int32_t n3 = Items::at(n1 - 1).rating + (this->itemData_[n] & 0xFF);
            string1 += "\nArmor value: " + std::to_string(n3);
            break;
        }
        case 11: {
            string1 = Items::at(n1 - 1).name + '\n' + Items::categoryNames_[by1 - 1];
            break;
        }
        case 12: {
            string1 = Items::at(n1 - 1).name + '\n' + "Spell: ";
            int32_t n4 = this->itemData_[n] & 0xFF;
            string1 = string1 + Spell::all_[n4 - 1]->name_;
            if (!profile_->knownSpellCheck) break;
            if ((this->knownSpells_ & (1 << (n4 - 1))) == 0) break;
            string1 = string1 + " (known)";
            break;
        }
        case 13: {
            int32_t n5 = n1 - 87;
            string1 = Items::at(n1 - 1).name + '\n' + Items::categoryNames_[by1 - 1] + '\n' +
                      extension_->itemEffectText(n5);
            break;
        }
        default: {
            if (by1 == profile_->conjuredWeaponCategory) {
                string1 = Items::at(n1 - 1).name + '\n' + Items::categoryNames_[by1 - 1];
                int32_t n6 = this->skillRank(skills::CONJURATION, false);
                int32_t n7 = 20 + n6;
                string1 += "\nWeapon value: " + std::to_string(n7);
                break;
            }
            string1 = Items::at(n1 - 1).name + '\n' + Items::categoryNames_[by1 - 1];
        }
    }
    return string1;
}

bool Player::learnSpell(int32_t n) {
    int32_t n1 = wrappingAbs(this->inventory_[n]);
    int8_t by1 = Items::at(n1 - 1).category;
    (void)by1;
    int32_t n2 = this->itemData_[n] & 0xFF;
    int32_t n3 = n2 - 1;
    this->knownSpells_ = GameUtil::setBit(n3, this->knownSpells_);
    this->removeItem(n);
    return true;
}

void Player::useItemOnTarget(int32_t n) {
    this->useItem(n, world_->combatTarget());
}

bool Player::isEquippable(int32_t n) {
    int32_t n1 = wrappingAbs(this->inventory_[n]);
    int8_t by1 = Items::at(n1 - 1).category;
    if (by1 >= 1 && by1 <= 10) {
        return true;
    }
    return by1 == profile_->conjuredWeaponCategory;
}

bool Player::isUsable(int32_t n) {
    int32_t n1 = wrappingAbs(this->inventory_[n]);
    int8_t by1 = Items::at(n1 - 1).category;
    if (by1 == 13) {
        return true;
    }
    return profile_->secondUsableCategory != 0 && by1 == profile_->secondUsableCategory;
}

bool Player::canLearn(int32_t n) {
    int32_t n1 = wrappingAbs(this->inventory_[n]);
    int8_t by1 = Items::at(n1 - 1).category;
    switch (by1) {
        case 12: {
            int32_t n2 = this->itemData_[n] & 0xFF;
            int8_t by2 = Spell::all_[n2 - 1]->skill_;
            if (profile_->knownSpellCheck && (this->knownSpells_ & (1 << (n2 - 1))) != 0) {
                return false;
            }
            return this->skills_[by2][0] > 0;
        }
    }
    return false;
}

int32_t Player::effectiveVital(int32_t n) {
    return vitals::effectiveVital(this->vitals_, n,
                                  this->spellActive(vitals::kFortifyVitalsSpell),
                                  this->skillRank(vitals::kRestorationSkill, false));
}

bool Player::hasRecallPoint() {
    return this->recallDungeon_ > 0;
}

void Player::warpToCamp(bool bl) {
    if (!bl) {
        this->recallDungeon_ = this->dungeonId_;
        this->recallX_ = this->gridX_;
        this->recallY_ = this->gridY_;
        this->recallFacing_ = this->facing_;
    }
    platform::writeLogLine("Map: warping to camp (" +
                           (bl ? std::string("without a recall point")
                               : "recall point saved at dungeon " +
                                     std::to_string((int32_t)this->recallDungeon_)) +
                           ")");
    this->placeAtStart(true);
    this->warped_ = true;
}

void Player::recall() {
    platform::writeLogLine("Map: recalling to dungeon " +
                           std::to_string((int32_t)this->recallDungeon_) +
                           " at x=" + std::to_string((int32_t)this->recallX_) +
                           " y=" + std::to_string((int32_t)this->recallY_));
    this->dungeonId_ = this->nextDungeon_ = this->recallDungeon_;
    this->gridX_ = this->nextX_ = this->recallX_;
    this->gridY_ = this->nextY_ = this->recallY_;
    if (profile_->recallRestoresFacing) {
        this->facing_ = this->recallFacing_;
    }
    this->refreshSurroundings();
    this->warped_ = true;
    if (profile_->placementRefreshesCanvas) {
        world_->refreshAhead();
    }
}

int32_t Player::ailmentCount() {
    int32_t n1 = 0;
    int32_t n2 = 0;
    while (n2 < 8) {
        int32_t n3 = (this->ailments_ >> n2) & 1;
        if (n3 != 0) {
            ++n1;
        }
        ++n2;
    }
    return n1;
}

void Player::cureOneAilment() {
    int32_t n1 = this->ailmentCount();
    if (n1 > 0) {
        int32_t n2 = 0;
        n2 = n1 == 1 ? 1 : GameUtil::randomInt(world_->worldState().random, n1);
        int32_t n3 = 0;
        int32_t n4 = 0;
        while (n4 < 8) {
            int32_t n5 = (this->ailments_ >> n4) & 1;
            if (n5 == 1 && ++n3 == n2) {
                this->ailments_ = (int8_t)GameUtil::clearBit(n4, (int32_t)this->ailments_);
                break;
            }
            ++n4;
        }
    }
}

int32_t Player::fatigueMultiplier() {
    return vitals::fatigueMultiplier(this->ailments_);
}

DungeonCore *Player::dungeon() {
    return world_->dungeonAt(this->dungeonId_);
}

void Player::refreshSurroundings() {
    this->dungeon()->scanSurroundings(this->gridX_, this->gridY_, this->facing_, this->surroundings_);
}

std::string Player::characterSheet() {
    std::string out;
    out.reserve(900);
    const char *spacer = profile_->sheetSpacer;
    out += this->name_;
    out += '\n';
    out += classNames_[this->classId_];
    out += '\n';
    out += "Level ";
    out += std::to_string((int32_t)this->vitals_[0]);
    out += " (";
    out += std::to_string((int32_t)this->vitals_[1]);
    out += "/10)\nHealth: ";
    out += std::to_string(this->effectiveVital(2));
    out += '/';
    out += std::to_string((int32_t)this->vitals_[3]);
    out += '\n';
    out += profile_->magickaLabel;
    out += std::to_string(this->effectiveVital(4));
    out += '/';
    out += std::to_string((int32_t)this->vitals_[5]);
    out += "\nFatigue: ";
    out += std::to_string(this->effectiveVital(6));
    out += '/';
    out += std::to_string((int32_t)this->vitals_[7]);
    out += '\n';
    out += spacer;
    out += "\nStatus ailments: ";
    int32_t n1 = 0;
    for (int32_t n2 = 1; n2 <= 8; ++n2) {
        if (this->hasAilment(n2)) {
            out += '\n';
            out += profile_->ailmentNames[n2 - 1];
            ++n1;
        }
    }
    if (n1 == 0) {
        out += "\nNone";
    }
    out += '\n';
    out += spacer;
    out += "\nGift points found: ";
    out += std::to_string((int32_t)this->giftPoints_);
    out += '\n';
    out += spacer;
    out += "\nAttributes:\n";
    for (int32_t n3 = 0; n3 < 8; ++n3) {
        int32_t n4 = 2 * n3;
        out += attributeNames_[n4];
        out += ": ";
        out += std::to_string((int32_t)this->attributes_[n4]);
        out += '\n';
    }
    return std::string(out);
}

std::vector<std::string> Player::skillList() {
    std::vector<std::string> skills;
    int32_t n1 = 0;
    while (n1 < 14) {
        if (this->skills_[n1][0] > 0) {
            std::string string1 =
                skillNames_[n1] + ": " + std::to_string((int32_t)this->skills_[n1][0]);
            skills.push_back(string1);
        }
        ++n1;
    }
    return skills;
}

int32_t Player::skillAt(int32_t n) {
    return inventory::skillAt(this->skills_, n);
}

std::string Player::describeSkill(int32_t n) {
    std::string string1 = skillNames_[n] + '\n' + "Rank: " +
                          std::to_string((int32_t)this->skills_[n][0]) + '\n' + "Exp: " +
                          std::to_string((int32_t)this->skills_[n][2]) + "/10";
    return string1;
}

std::vector<std::string> Player::spellList() {
    std::vector<std::string> spells;
    int32_t n1 = 0;
    while (n1 < Spell::count_) {
        if ((this->knownSpells_ & (1 << n1)) != 0) {
            int32_t n2 = n1 + 1;
            std::string string1 = Spell::all_[n1]->name_;
            if (n2 == this->readiedSpell_) {
                string1 = std::string("R: ") + string1;
            }
            spells.push_back(string1);
        }
        ++n1;
    }
    return spells;
}

int32_t Player::spellAt(int32_t n) {
    int32_t n1 = 0;
    int32_t n2 = 0;
    while (n2 < Spell::count_) {
        if ((this->knownSpells_ & (1 << n2)) != 0) {
            if (n1 == n) {
                return n2;
            }
            ++n1;
        }
        ++n2;
    }
    return -1;
}

int32_t Player::nextSpell() {
    if (!Spell::isValidId(this->readiedSpell_)) {
        int32_t n1 = this->spellAt(0);
        if (n1 < 0) {
            return 0;
        }
        return n1 + 1;
    }
    int32_t n2 = this->readiedSpell_ - 1;
    int32_t n3 = n2 + 1;
    if (n3 == Spell::count_) {
        n3 = 0;
    }
    while (n3 != n2) {
        if ((this->knownSpells_ & (1 << n3)) != 0) {
            return n3 + 1;
        }
        if (++n3 != Spell::count_) continue;
        n3 = 0;
    }
    return this->readiedSpell_;
}

std::string Player::describeSpell(int32_t n) {
    return smallhelpers::describeSpell(skillNames_, n);
}

void Player::rest(bool bl) {
    int32_t n1;
    int32_t n2;
    extension_->onRest(*this);
    int16_t s1 = (int16_t)(this->vitals_[3] - this->vitals_[2]);
    int16_t s2 = (int16_t)(this->vitals_[5] - this->vitals_[4]);
    int16_t s3 = (int16_t)(this->vitals_[7] - this->vitals_[6]);
    if (!bl) {
        s1 = (int16_t)(2 * s1 / 3);
        s2 = (int16_t)(2 * s2 / 3);
        s3 = (int16_t)(2 * s3 / 3);
    }
    if (profile_->restClearsCounters) {
        this->vitals_[9] = 0;
        this->vitals_[8] = 0;
    }
    if (this->hasAilment(8)) {
        s1 = (int16_t)(3 * s1 / 4);
        s2 = (int16_t)(3 * s2 / 4);
        s3 = (int16_t)(3 * s3 / 4);
    }
    this->vitals_[2] = (int16_t)(this->vitals_[2] + s1);
    this->vitals_[4] = (int16_t)(this->vitals_[4] + s2);
    this->vitals_[6] = (int16_t)(this->vitals_[6] + s3);
    this->potionAttack_ = false;
    this->potionDefence_ = false;
    this->potionEscape_ = false;
    GameRandom *random = world_->worldState().random;
    int32_t n3 = GameUtil::randomInt(random, 100);
    if (n3 <= 10) {
        n2 = 0;
        while (n2 < this->itemCount_) {
            n1 = wrappingAbs(this->inventory_[n2]);
            if (n1 == 96) {
                this->removeItem(n2);
                break;
            }
            ++n2;
        }
    }
    n2 = 0;
    while (n2 < 8) {
        n1 = n2 + 1;
        if (n1 != 4 && n1 != 5 && (n3 = GameUtil::randomInt(random, 100)) <= 25) {
            this->ailments_ = (int8_t)GameUtil::clearBit(n2, (int32_t)this->ailments_);
        }
        ++n2;
    }
}

bool Player::hasAilment(int32_t n) {
    int32_t n1 = n - 1;
    return (this->ailments_ & (1 << n1)) != 0;
}

void Player::useItem(int32_t n, Monster *d2) {
    int32_t n1 = wrappingAbs(this->inventory_[n]);
    int8_t by1 = Items::at(n1 - 1).category;
    const bool usable = by1 == 13 || (profile_->secondUsableCategory != 0 &&
                                      by1 == profile_->secondUsableCategory);
    if (usable) {
        bool bl1 = true;
        switch (n1) {
            case 87: {
                if (this->dungeonId_ == 1 && this->hasRecallPoint()) {
                    this->recall();
                    break;
                }
                this->warpToCamp(false);
                break;
            }
            case 88: {
                this->cureOneAilment();
                break;
            }
            case 89: {
                this->vitals_[2] = this->vitals_[3];
                break;
            }
            case 90: {
                this->vitals_[4] = this->vitals_[5];
                break;
            }
            case 91: {
                this->vitals_[6] = (int16_t)(this->vitals_[6] + 3 * this->vitals_[5]);
                break;
            }
            case 92: {
                this->vitals_[1] = (int16_t)(this->vitals_[1] + 1);
                break;
            }
            case 93: {
                this->vitals_[2] = this->vitals_[3];
                this->vitals_[4] = this->vitals_[5];
                break;
            }
            case 94: {
                this->potionAttack_ = true;
                break;
            }
            case 95: {
                this->potionDefence_ = true;
                break;
            }
            case 96: {
                this->potionEscape_ = true;
                bl1 = false;
                break;
            }
            case 97: {
                if (d2 == nullptr) break;
                int32_t n2 = d2->stat(4);
                int32_t n3 = d2->stat(10);
                if (n2 > 13 || n3 > 13) break;
                d2->hp_ = 0;
                d2->store();
                break;
            }
            case 98: {
                if (d2 == nullptr) break;
                int32_t n4 = d2->stat(4);
                int32_t n5 = d2->stat(10);
                if (n4 > 22 || n5 > 22) break;
                d2->hp_ = 0;
                d2->store();
                break;
            }
            case 99: {
                if (d2 == nullptr) break;
                int32_t n6 = d2->stat(4);
                int32_t n7 = d2->stat(10);
                if (n6 > 29 || n7 > 29) break;
                d2->hp_ = 0;
                d2->store();
            }
        }
        if (bl1) {
            this->removeItem(n);
        }
    }
}

void Player::spendLevelUp() {
    if (profile_->spendClearsLevelUpMask) {
        this->levelUpMask_ = 0;
    }
    world_->worldState().npcs.resetPerLevel();
    this->vitals_[1] = (int16_t)(this->vitals_[1] - 10);
}

SharedArray<std::string> Player::levelUpChoices() {
    int32_t n1;
    std::vector<std::string> choices;
    int32_t n2 = 0;
    while (n2 < 8) {
        if ((this->levelUpMask_ & (1 << n2)) != 0) {
            n1 = n2 * 2;
            choices.push_back(attributeNames_[n1]);
        }
        ++n2;
    }
    n1 = (int32_t)choices.size();
    if (n1 == 0) {
        return SharedArray<std::string>();
    }
    SharedArray<std::string> stringArray1(n1);
    int32_t n3 = 0;
    while (n3 < n1) {
        stringArray1[n3] = choices[(size_t)n3];
        ++n3;
    }
    return stringArray1;
}

void Player::giveStartingKit() {
    int16_t s1 = Items::nextId();
    const int32_t *nArray1 = startingKit_[this->classId_];
    int32_t len = 2;
    int32_t n1 = 0;
    while (n1 < len) {
        this->addItem(nArray1[n1], s1, 0);
        int32_t n2 = this->itemCount_ - 1;
        this->equip(n2, true);
        ++n1;
    }
}

void Player::regenFatigue(int64_t l) {
    int32_t n1 = GameFormulas::calcFatigueRegen(l, this->attributes_[10], this->attributes_[11]);
    this->vitals_[6] = (int16_t)(this->vitals_[6] + n1);
    if (this->vitals_[6] > this->vitals_[7]) {
        this->vitals_[6] = this->vitals_[7];
    }
}

int32_t Player::npcInteractionCheck(int32_t faction, int32_t n2) {
    int32_t n1 = this->skillRank(skills::SPEECHCRAFT, true);
    if (n2 == 3) {
        n1 += 3;
    }
    worldstate::WorldState &worldState = world_->worldState();
    int16_t s1 = worldState.npcs.interactionCount[faction];
    return GameFormulas::calcInteractionCheck(worldState.random, n1, s1, this->attributes_[12]);
}
