#include "src/common/game/monster.hpp"

#include "src/common/game/formulas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/monsterdata.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/datfiles.hpp"

int32_t Monster::typeCount_ = 0;
SharedArray<std::string> Monster::typeNames_;
SharedArray<SharedArray<int8_t>> Monster::typeStats_;

namespace {

monsterdata::TypeTable typeTable() {
    monsterdata::TypeTable table;
    table.count = Monster::typeCount_;
    table.names = Monster::typeNames_;
    table.stats = Monster::typeStats_;
    return table;
}

monsterdata::Fields collectFields(const Monster *m) {
    monsterdata::Fields fields;
    fields.uid = m->uid_;
    fields.type = m->type_;
    fields.hp = m->hp_;
    fields.gridX = m->gridX_;
    fields.gridY = m->gridY_;
    fields.seen = m->seen_;
    fields.dungeonId = m->dungeonId_;
    fields.moveCounter = m->moveCounter_;
    fields.attackPhase = m->attackPhase_;
    fields.lastActionMs = m->lastActionMs_;
    for (int32_t i = 0; i < monsterdata::kEffectCount; ++i) {
        fields.effects[i] = m->effects_[i];
    }
    return fields;
}

void applyFields(Monster *m, const monsterdata::Fields &fields) {
    m->uid_ = fields.uid;
    m->type_ = fields.type;
    m->hp_ = fields.hp;
    m->gridX_ = fields.gridX;
    m->gridY_ = fields.gridY;
    m->seen_ = fields.seen;
    m->dungeonId_ = fields.dungeonId;
    m->moveCounter_ = fields.moveCounter;
    m->attackPhase_ = fields.attackPhase;
    m->lastActionMs_ = fields.lastActionMs;
    for (int32_t i = 0; i < monsterdata::kEffectCount; ++i) {
        m->effects_[i] = fields.effects[i];
    }
}

}

Monster::Monster() {
    this->effects_ = SharedArray<int8_t>(10);
    this->seen_ = false;
}

Monster::Monster(int32_t n, int32_t n2, int32_t n3) {
    this->uid_ = (int16_t)n;
    this->type_ = (int8_t)n2;
    this->hp_ = typeStats_[this->type_ - 1][14];
    this->effects_ = SharedArray<int8_t>(10);
    this->seen_ = false;
    this->dungeonId_ = (int8_t)n3;
    this->attackPhase_ = 0;
}

SharedArray<int8_t> Monster::toRecord() {
    SharedArray<int8_t> byArray1(28);
    byArray1[0] = (int8_t)(unsignedShiftRight32(this->uid_, 8) & 0xFF);
    byArray1[1] = (int8_t)(this->uid_ & 0xFF);
    byArray1[2] = this->type_;
    byArray1[3] = this->hp_;
    byArray1[4] = this->gridX_;
    byArray1[5] = this->gridY_;
    byArray1[6] = this->seen_ ? (int8_t)1 : (int8_t)0;
    byArray1[7] = this->dungeonId_;
    byArray1[8] = this->moveCounter_;
    byArray1[9] = this->attackPhase_;
    byArray1[10] = (int8_t)(unsignedShiftRight64(this->lastActionMs_, 56) & 0xFFL);
    byArray1[11] = (int8_t)(unsignedShiftRight64(this->lastActionMs_, 48) & 0xFFL);
    byArray1[12] = (int8_t)(unsignedShiftRight64(this->lastActionMs_, 40) & 0xFFL);
    byArray1[13] = (int8_t)(unsignedShiftRight64(this->lastActionMs_, 32) & 0xFFL);
    byArray1[14] = (int8_t)(unsignedShiftRight64(this->lastActionMs_, 24) & 0xFFL);
    byArray1[15] = (int8_t)(unsignedShiftRight64(this->lastActionMs_, 16) & 0xFFL);
    byArray1[16] = (int8_t)(unsignedShiftRight64(this->lastActionMs_, 8) & 0xFFL);
    byArray1[17] = (int8_t)(this->lastActionMs_ & 0xFFL);
    int32_t n1 = 0;
    while (n1 < 10) {
        byArray1[18 + n1] = this->effects_[n1];
        ++n1;
    }
    return byArray1;
}

Monster *Monster::fromRecord(Monster *d2, const SharedArray<int8_t> &byArray, DungeonCore *dungeon) {
    int16_t s1 = (int16_t)(byArray[0] & 0xFF);
    int16_t s2 = (int16_t)(byArray[1] & 0xFF);
    d2->uid_ = (int16_t)((s1 << 8) | s2);
    d2->type_ = byArray[2];
    d2->hp_ = byArray[3];
    d2->gridX_ = byArray[4];
    d2->gridY_ = byArray[5];
    d2->seen_ = byArray[6] != 0;
    d2->dungeonId_ = byArray[7];
    d2->moveCounter_ = byArray[8];
    d2->attackPhase_ = byArray[9];
    d2->lastActionMs_ = GameUtil::readLongBE(byArray, 10);
    d2->dungeon_ = dungeon;
    int32_t n1 = 0;
    while (n1 < 10) {
        d2->effects_[n1] = byArray[18 + n1];
        ++n1;
    }
    return d2;
}

void Monster::store() {
    dungeon_->worldState_.monsters.put((std::size_t)(this->dungeonId_ - 1), this->toRecord());
}

std::string Monster::name() { return monsterdata::typeName(typeTable(), this->type_); }

int32_t Monster::stat(int32_t n) { return monsterdata::typeStat(typeTable(), this->type_, n); }

bool Monster::isUndead() { return monsterdata::isUndeadType(this->type_); }

void Monster::takeDamage(int32_t n) {
    this->hp_ = (int8_t)monsterdata::applyDamage(this->hp_ & 0xFF, n);
}

bool Monster::stepDir(int32_t n) {
    int32_t n1 = 1;
    int8_t by1 = this->gridX_;
    int8_t by2 = this->gridY_;
    DungeonCore *i2 = dungeon_;
    switch (n) {
        case 1: {
            n1 = -1;
        }
        case 3: {
            by1 = this->gridX_;
            by2 = (int8_t)(this->gridY_ + n1);
            break;
        }
        case 4: {
            n1 = -1;
        }
        case 2: {
            by2 = this->gridY_;
            by1 = (int8_t)(this->gridX_ + n1);
            break;
        }
        default: {
            return false;
        }
    }
    if (!i2->isFree(by1, by2)) {
        return false;
    }
    if (this->isEntranceTile(by1, by2)) {
        return false;
    }
    const bool coordinateKeyed =
        i2->worldState_.monsters.key() == worldstate::MonsterKey::Coordinates;
    if (coordinateKeyed) {
        i2->worldState_.monsters.removeAt((std::size_t)(this->dungeonId_ - 1), this->gridX_,
                                          this->gridY_);
    }
    i2->tiles_[this->gridX_][this->gridY_] =
        GameUtil::clearFlag((int8_t)2, i2->tiles_[this->gridX_][this->gridY_]);
    i2->tiles_[by1][by2] = GameUtil::setFlag((int8_t)2, i2->tiles_[by1][by2]);
    this->gridX_ = by1;
    this->gridY_ = by2;
    if (coordinateKeyed) {
        this->store();
    }
    return true;
}

bool Monster::takeTurn(int32_t n, int32_t n2) {
    bool bl1 = false;
    if (this->moveCounter_ == 0) {
        this->pursue(n, n2);
        this->moveCounter_ = (int8_t)(this->moveCounter_ + 1);
        bl1 = true;
    } else {
        this->moveCounter_ = this->moveCounter_ >= 4 ? (int8_t)0 : (int8_t)(this->moveCounter_ + 1);
    }
    this->attackPhase_ = 0;
    this->store();
    return bl1;
}

void Monster::takeTurn(Combatant *j2) {
    if (this->isInRange(j2)) {
        if (this->moveCounter_ == 0) {
            this->pursue(j2->gridX(), j2->gridY());
            this->moveCounter_ = (int8_t)(this->moveCounter_ + 1);
        } else {
            this->moveCounter_ = this->moveCounter_ >= 4 ? (int8_t)0 : (int8_t)(this->moveCounter_ + 1);
        }
    }
}

void Monster::pursue(int32_t n, int32_t n2) {
    int32_t n1;
    int32_t n3;
    int32_t n4 = wrappingAbs(n - this->gridX_);
    int32_t n5 = wrappingAbs(n2 - this->gridY_);
    int32_t n6 = this->gridX_ < n ? 2 : (this->gridX_ > n ? 4 : -1);
    int32_t n7 = this->gridY_ < n2 ? 3 : (this->gridY_ > n2 ? 1 : -1);
    if (n4 > n5) {
        n3 = n6;
        n1 = n7;
    } else if (n4 < n5) {
        n3 = n7;
        n1 = n6;
    } else {
        int32_t n8 = wrappingAbs(dungeon_->worldState_.random->nextInt() % 2);
        if (n8 == 0) {
            n3 = n6;
            n1 = n7;
        } else {
            n3 = n7;
            n1 = n6;
        }
    }
    if (this->stepDir(n3)) {
        return;
    }
    if (this->stepDir(n1)) {
        return;
    }
}

bool Monster::isEntranceTile(int32_t n, int32_t n2) {
    DungeonCore *i2 = dungeon_;
    return i2->exitDir1_ == 1 || i2->exitDir2_ == 1
               ? n == 17 && n2 == 5
               : (i2->exitDir1_ == 3 || i2->exitDir2_ == 3
                      ? n == 17 && n2 == 30
                      : (i2->exitDir1_ == 4 || i2->exitDir2_ == 4
                             ? n == 5 && n2 == 17
                             : (i2->exitDir1_ == 2 || i2->exitDir2_ == 2) && n == 30 && n2 == 17));
}

bool Monster::isInRange(Combatant *j2) { return this->distanceTo(j2) <= 3; }

int32_t Monster::distanceTo(Combatant *j2) {
    int32_t n1 = wrappingAbs(j2->gridX() - this->gridX_);
    int32_t n2 = wrappingAbs(j2->gridY() - this->gridY_);
    return n1 + n2;
}

bool Monster::isAdjacent(Combatant *j2) {
    if (this->distanceTo(j2) == 1) {
        return true;
    }
    this->attackPhase_ = 0;
    return false;
}

bool Monster::attack(Combatant *j2, int64_t l) {
    int8_t by1;
    bool bl1 = false;
    if (this->attackPhase_ == 0) {
        this->lastActionMs_ = l;
        this->attackPhase_ = 1;
    } else if (this->attackPhase_ == 1 && l - this->lastActionMs_ > 800L) {
        bl1 = true;
    }
    if (!bl1) {
        return false;
    }
    this->attackPhase_ = (int8_t)2;
    this->lastActionMs_ = l;
    int8_t by2 = typeStats_[this->type_ - 1][4];
    int32_t n1 = j2->defenceSkill(true);
    int32_t n2 = n1 - by2;
    n2 = min32(n2, typeStats_[this->type_ - 1][2]);
    int32_t n3 = GameFormulas::clampChance(typeStats_[this->type_ - 1][3] - n2 * 5);
    int32_t n4 = GameFormulas::clampChance(j2->defenceAptitude() + n2 * 5);
    bool bl2 = false;
    int32_t n5 = GameFormulas::resolveCombatRoll(dungeon_->worldState_.random, n3, n4, &bl2);
    if (n5 == 0) {
        this->attackPhase_ = 1;
        return false;
    }
    int8_t by3 = typeStats_[this->type_ - 1][5];
    int32_t n6 = j2->armourRating();
    if (n5 == 1) {
        n6 = 2 * n6;
    }
    SharedArray<int16_t> vitals = j2->vitals();
    int32_t n7 = GameFormulas::calcDamage(by3, n6, vitals[3]);
    vitals[2] = (int16_t)(vitals[2] - n7);
    vitals[2] = (int16_t)max32(vitals[2], 0);
    if (bl2) {
        j2->awardSkillXp(j2->defenceSkillIndex(), 1);
    }
    if (n5 < 3) {
        this->attackPhase_ = 1;
        return true;
    }
    if (GameUtil::randomInt(dungeon_->worldState_.random, 100) <= 30 &&
        (by1 = typeStats_[this->type_ - 1][11]) > 0) {
        int32_t n8 = by1 - 1;
        j2->setAilments((int8_t)(j2->ailments() | (1 << n8)));
        if (by1 != 1) {
            if (by1 == 2) {
                DungeonCore *i2 = dungeon_;
                i2->spawnMonsters(3);
            } else if (by1 != 3) {
                if (by1 == 4) {
                    j2->setAilmentTimer(4, (int16_t)30000);
                } else if (by1 == 5) {
                    j2->setAilmentTimer(5, (int16_t)30000);
                } else if (by1 == 6 || by1 == 7 || by1 == 8) {
                }
            }
        }
    }
    this->attackPhase_ = 1;
    return true;
}

void Monster::loadTypes(platform::PlatformContext *context) {
    BinaryReader *dataInputStream = GameUtil::openResource(context, std::string("/monstersin.dat"));
    datfiles::loadMonsters(dataInputStream, &typeCount_, &typeNames_, &typeStats_);
    delete dataInputStream;
}

Monster *Monster::readFrom(BinaryReader *dataInputStream) {
    monsterdata::Fields fields;
    monsterdata::readFields(dataInputStream, &fields);
    Monster *d2 = new Monster();
    applyFields(d2, fields);
    return d2;
}

void Monster::writeTo(BinaryWriter *dataOutputStream) {
    monsterdata::writeFields(dataOutputStream, collectFields(this));
}

void Monster::dropLoot(bool bl) {
    bool bl1;
    int32_t n1 = typeStats_[this->type_ - 1][15];
    if (bl) {
        n1 = 100;
    }
    int8_t by1 = typeStats_[this->type_ - 1][16];
    int32_t n2 = GameUtil::randomInt(dungeon_->worldState_.random, 100);
    bl1 = n2 <= n1;
    if (bl1 || bl) {
        DungeonCore *i2 = dungeon_;
        int8_t by2 = i2->level_;
        int32_t n3 = Items::randomDropped(dungeon_->worldState_.random, by2, by1);
        int8_t by3 = (int8_t)(n3 & 0xFF);
        int8_t by4 = 0;
        if (by3 == 86) {
            by4 = (int8_t)(unsignedShiftRight32(n3, 8) & 0xFF);
        }
        SharedArray<int8_t> byArray1(7);
        byArray1[0] = this->gridX_;
        byArray1[1] = this->gridY_;
        byArray1[2] = by3;
        byArray1[5] = by4;
        int16_t s1 = Items::nextId();
        by3 = (int8_t)(unsignedShiftRight32(s1, 8) & 0xFF);
        by4 = (int8_t)(s1 & 0xFF);
        byArray1[3] = by3;
        byArray1[4] = by4;
        byArray1[6] = 1;
        if (bl) {
            byArray1[6] = (int8_t)(byArray1[6] | 4);
        }
        i2->addDroppedItem(byArray1);
    }
}

Monster *Monster::spawn(GameRandom *random, int32_t n, DungeonCore *dungeon, int32_t n3) {
    int16_t s1 = dungeon->worldState_.nextMonsterUid();
    int32_t n1 = n3;
    if (n1 < 0) {
        int32_t n4 = n - 1;
        if (n4 < 0) {
            n4 = 0;
        }
        if (n4 > 36) {
            n4 = 36;
        }
        int32_t n5 = GameUtil::randomInt(random, 10);
        int32_t n6 = 0;
        n6 = n5 <= 4 ? 0 : (n5 <= 7 ? 1 : (n5 <= 9 ? 2 : 3));
        n1 = DungeonCore::kSpawnTable[n4][n6];
    }
    Monster *d2 = new Monster(s1, n1, dungeon->id_);
    d2->dungeon_ = dungeon;
    return d2;
}

Monster *Monster::spawn(GameRandom *random, DungeonCore *i2, int32_t n) {
    return Monster::spawn(random, i2->level_, i2, n);
}

Monster *Monster::spawnDefault(DungeonCore *i2) {
    return Monster::spawn(i2->worldState_.random, i2, -1);
}
