#include "src/stormhold/dungeon.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/monster.hpp"
#include "src/stormhold/npc_script.hpp"
#include "src/common/game/player.hpp"

namespace stormhold {

SharedArray<SharedArray<std::string>> Dungeon::dungeonNames_;

const int32_t Dungeon::kChestTier[36] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 3, 1, 3, 3,
                        1, 3, 3, 3, 3, 5, 3, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5};

Dungeon::Dungeon(worldstate::DungeonRegistry &dungeons,
                 worldstate::WorldState &worldState)
    : DungeonCore(worldState, false), dungeons_(dungeons) {}

Dungeon::Dungeon(worldstate::DungeonRegistry &dungeons,
                 worldstate::WorldState &worldState, int8_t by,
                 const SharedArray<int8_t> &byArray)
    : Dungeon(dungeons, worldState) {
    this->id_ = by;
    this->assignLevel();
    this->width_ = (int16_t)35;
    this->height_ = (int16_t)35;
    this->keyScratch_ = SharedArray<int16_t>(2);
    this->roomScratch_ = SharedArray<int16_t>(6);
    this->geometry_ = byArray;
    this->exitDir1_ = this->geometry_[4];
    this->exitDir2_ = this->geometry_[5];
}

Dungeon::Dungeon(worldstate::DungeonRegistry &dungeons,
                 worldstate::WorldState &worldState, int8_t by,
                 const SharedArray<int8_t> &byArray, int32_t n, int32_t n2,
                 SharedArray<SharedArray<int8_t>> byArray2)
    : Dungeon(dungeons, worldState) {
    this->id_ = by;
    this->assignLevel();
    this->width_ = (int16_t)n;
    this->height_ = (int16_t)n2;
    this->tiles_ = byArray2;
    this->keyScratch_ = SharedArray<int16_t>(2);
    this->roomScratch_ = SharedArray<int16_t>(6);
    this->geometry_ = byArray;
    this->exitDir1_ = this->geometry_[4];
    this->exitDir2_ = this->geometry_[5];
    int32_t n1 = 6;
    if (worldState_.npcs.wardenPresent) {
        ++n1;
    }
    int32_t n3 = 0;
    while (n3 < n1) {
        SharedArray<int8_t> byArray1 = this->tiles_[NpcSystem::npcGridX_[n3]];
        int8_t by1 = NpcSystem::npcGridY_[n3];
        byArray1[by1] = (int8_t)(byArray1[by1] | 0x20);
        ++n3;
    }
    this->populated_ = true;
}

void Dungeon::spawnRoomMonsters() {
    int32_t n1 = (int32_t)this->rooms_.size();
    int16_t s1 = (int16_t)(this->id_ << 8);
    (void)s1;
    Monster *d2 = nullptr;
    int32_t n2 = 0;
    while (n2 < n1) {
        SharedArray<int16_t> sArray1 = this->rooms_[(size_t)n2];
        d2 = this->id_ == 37 && n2 == n1 - 1 ? Monster::spawn(this->rng_, this, 41) : Monster::spawn(this->rng_, this, -1);
        this->placeMonster(d2, sArray1);
        d2->store();
        ++n2;
    }
}

void Dungeon::placeMonster(Monster *d2, SharedArray<int16_t> sArray) {
    d2->gridX_ = (int8_t)sArray[4];
    d2->gridY_ = (int8_t)sArray[5];
    SharedArray<int8_t> byArray1 = this->tiles_[d2->gridX_];
    int8_t by1 = d2->gridY_;
    byArray1[by1] = (int8_t)(byArray1[by1] | 2);
}

void Dungeon::spawnMonsters(int32_t n) {
    int32_t n1 = (int32_t)this->rooms_.size();
    int32_t n2 = 0;
    while (n2 < n) {
        Monster *d2;
        int16_t s1;
        int16_t s2;
        int16_t s3;
        SharedArray<int16_t> sArray1;
        int16_t s4;
        do {
            int32_t n3 = wrappingAbs(this->rng_->nextInt() % n1);
            d2 = Monster::spawnDefault(this);
            sArray1 = this->rooms_[(size_t)n3];
            s3 = (int16_t)(sArray1[2] - sArray1[0] + 1);
            s1 = (int16_t)(sArray1[3] - sArray1[1] + 1);
            s4 = (int16_t)(sArray1[0] + wrappingAbs(this->rng_->nextInt() % s3));
            s2 = (int16_t)(sArray1[1] + wrappingAbs(this->rng_->nextInt() % s1));
        } while (!this->isFree(s4, s2));
        d2->gridX_ = (int8_t)s4;
        d2->gridY_ = (int8_t)s2;
        this->tiles_[d2->gridX_][d2->gridY_] = GameUtil::setFlag((int8_t)2, this->tiles_[d2->gridX_][d2->gridY_]);
        d2->store();
        ++n2;
    }
}

void Dungeon::spawnNear(Player *j2) {
    if (this->id_ == 1) {
        return;
    }
    Monster *d2 = Monster::spawnDefault(this);
    int32_t n1 = j2->gridX_;
    int32_t n2 = j2->gridY_;
    int32_t n3 = 0;
    while (n3 <= 4) {
        int32_t n4 = n1;
        int32_t n5 = n2;
        if (n3 < 2) {
            n4 += 2 * n3 - 1;
        } else {
            n5 += 2 * n3 - 5;
        }
        if (this->isFree(n4, n5)) {
            d2->gridX_ = (int8_t)n4;
            d2->gridY_ = (int8_t)n5;
            d2->store();
            this->tiles_[d2->gridX_][d2->gridY_] = GameUtil::setFlag((int8_t)2, this->tiles_[d2->gridX_][d2->gridY_]);
            break;
        }
        ++n3;
    }
}

int32_t Dungeon::compareInt(int32_t n, int32_t n2, bool bl) {
    if (bl) {
        if (n < n2) {
            return -1;
        }
        if (n > n2) {
            return 1;
        }
        return 0;
    }
    if (n > n2) {
        return -1;
    }
    if (n < n2) {
        return 1;
    }
    return 0;
}

SharedArray<int32_t> Dungeon::pickRooms(int32_t n) {
    int32_t n1;
    int32_t n2;
    int32_t n3 = (int32_t)this->rooms_.size();
    SharedArray<int32_t> nArray1(n3);
    SharedArray<int32_t> nArray2(n3);
    int32_t n4 = 0;
    while (n4 < n3) {
        nArray1[n4] = n4;
        nArray2[n4] = n2 = GameUtil::randomInt(this->rng_, 1000);
        ++n4;
    }
    n2 = 1;
    while (n2 < n3) {
        int32_t n5 = nArray2[n2];
        n1 = nArray1[n2];
        int32_t n6 = n2 - 1;
        while (n6 >= 0 && Dungeon::compareInt(nArray2[n6], n5, false) > 0) {
            nArray2[n6 + 1] = nArray2[n6];
            nArray1[n6 + 1] = nArray1[n6];
            --n6;
        }
        nArray2[n6 + 1] = n5;
        nArray1[n6 + 1] = n1;
        ++n2;
    }
    SharedArray<int32_t> nArray3(n);
    n1 = 0;
    while (n1 < n) {
        nArray3[n1] = nArray1[n1];
        ++n1;
    }
    return nArray3;
}

void Dungeon::placeChests() {
    int32_t n1 = 0;
    SharedArray<int32_t> nArray1 = this->pickRooms(5);
    int32_t n2 = Dungeon::kChestTier[this->level_ - 1];
    bool bl1 = true;
    int32_t n3 = 0;
    while (n3 < 5) {
        int32_t n4 = nArray1[n3];
        SharedArray<int16_t> sArray1 = this->rooms_[(size_t)n4];
        if (bl1) {
            n1 = Items::randomOfTier(this->rng_, n2);
            bl1 = false;
        } else {
            n1 = Items::randomDropped(this->rng_, (int32_t)this->level_, 2);
        }
        int16_t s1 = (int16_t)(sArray1[2] - sArray1[0] + 1);
        int16_t s2 = (int16_t)(sArray1[3] - sArray1[1] + 1);
        int16_t s3 = (int16_t)(sArray1[0] + wrappingAbs(this->rng_->nextInt() % s1));
        int16_t s4 = (int16_t)(sArray1[1] + wrappingAbs(this->rng_->nextInt() % s2));
        while ((this->tiles_[s3][s4] & 8) != 0) {
            s3 = (int16_t)(sArray1[0] + wrappingAbs(this->rng_->nextInt() % s1));
            s4 = (int16_t)(sArray1[1] + wrappingAbs(this->rng_->nextInt() % s2));
        }
        SharedArray<int8_t> byArray1(8);
        byArray1[0] = (int8_t)s3;
        byArray1[1] = (int8_t)s4;
        byArray1[2] = bl1 ? (int8_t)1 : 0;
        int8_t by1 = (int8_t)(wrappingAbs(this->rng_->nextInt() % 3) << 6);
        byArray1[3] = (int8_t)(by1 | this->level_);
        int8_t by2 = (int8_t)(n1 & 0xFF);
        int8_t by3 = 0;
        if (by2 == 86) {
            by3 = (int8_t)(unsignedShiftRight32(n1, 8) & 0xFF);
        }
        byArray1[4] = by2;
        byArray1[7] = by3;
        int16_t s5 = Items::nextId();
        by2 = (int8_t)(unsignedShiftRight32(s5, 8) & 0xFF);
        by3 = (int8_t)(s5 & 0xFF);
        byArray1[5] = by2;
        byArray1[6] = by3;
        this->addChest(byArray1);
        ++n3;
    }
}

void Dungeon::populate() {
    this->spawnRoomMonsters();
    this->placeChests();
}

void Dungeon::rebuildOccupancy() {
    SharedArray<int8_t> byArray1;
    const std::size_t monsterDungeonIndex = (std::size_t)(this->id_ - 1);
    if (worldState_.monsters.hasTable(monsterDungeonIndex)) {
        Monster monster;
        for (const SharedArray<int8_t> &record : worldState_.monsters.at(monsterDungeonIndex)) {
            byArray1 = record;
            Monster *d2 = Monster::fromRecord(&monster, byArray1, this);
            SharedArray<int8_t> byArray2 = this->tiles_[d2->gridX_];
            int8_t by1 = d2->gridY_;
            byArray2[by1] = (int8_t)(byArray2[by1] | 2);
        }
    }
    const std::size_t dungeonIndex = (std::size_t)(this->id_ - 1);
    if (worldState_.chests.hasTable(dungeonIndex)) {
        for (const SharedArray<int8_t> &chest : worldState_.chests.at(dungeonIndex)) {
            byArray1 = chest;
            SharedArray<int8_t> byArray3 = this->tiles_[byArray1[0]];
            int8_t by2 = byArray1[1];
            byArray3[by2] = (int8_t)(byArray3[by2] | 0x10);
        }
    }
    for (const SharedArray<int8_t> &dropped :
         worldState_.droppedItems.at((std::size_t)(this->id_ - 1))) {
        byArray1 = dropped;
        SharedArray<int8_t> byArray4 = this->tiles_[byArray1[0]];
        int8_t by3 = byArray1[1];
        byArray4[by3] = (int8_t)(byArray4[by3] | 4);
    }
    if (this->id_ == 1 && worldState_.npcs.wardenPresent) {
        int8_t by4 = NpcSystem::npcGridX_[6];
        int8_t by5 = NpcSystem::npcGridY_[6];
        SharedArray<int8_t> byArray5 = this->tiles_[by4];
        int8_t by6 = by5;
        byArray5[by6] = (int8_t)(byArray5[by6] | 0x20);
    }
}

Monster *Dungeon::monsterAt(Monster *target, int32_t n, int32_t n2) {
    int8_t by1 = this->tiles_[n][n2];
    if (GameUtil::hasFlag((int8_t)1, by1)) {
        return nullptr;
    }
    if (!GameUtil::hasFlag((int8_t)2, by1)) {
        return nullptr;
    }
    for (const SharedArray<int8_t> &byArray1 :
         worldState_.monsters.at((std::size_t)(this->id_ - 1))) {
        Monster *d2 = Monster::fromRecord(target, byArray1, this);
        if (d2->gridX_ != n || d2->gridY_ != n2) continue;
        return d2;
    }
    return nullptr;
}

void Dungeon::removeDroppedItem(const SharedArray<int8_t> &byArray) {
    int8_t by1 = byArray[0];
    int8_t by2 = byArray[1];
    int8_t by3 = this->tiles_[by1][by2];
    if (GameUtil::hasFlag((int8_t)1, by3)) {
        return;
    }
    if (!GameUtil::hasFlag((int8_t)4, by3)) {
        return;
    }
    worldState_.droppedItems.removeSame((std::size_t)(this->id_ - 1), byArray);
    if (this->countDroppedItems((int32_t)by1, (int32_t)by2) == 0) {
        this->tiles_[by1][by2] = GameUtil::clearFlag((int8_t)4, this->tiles_[by1][by2]);
    }
}

int32_t Dungeon::countDroppedItems(int32_t n, int32_t n2) {
    int32_t n1 = 0;
    for (const SharedArray<int8_t> &byArray1 :
         worldState_.droppedItems.at((std::size_t)(this->id_ - 1))) {
        if (byArray1[0] != n || byArray1[1] != n2) continue;
        ++n1;
    }
    return n1;
}

SharedArray<int8_t> Dungeon::firstDroppedItem(int32_t n, int32_t n2) {
    for (const SharedArray<int8_t> &byArray1 :
         worldState_.droppedItems.at((std::size_t)(this->id_ - 1))) {
        if (byArray1[0] != n || byArray1[1] != n2) continue;
        return byArray1;
    }
    return SharedArray<int8_t>();
}

void Dungeon::b(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray) {
    if (n3 == 1 || n3 == 3) {
        int32_t n1 = 0;
        n1 = n3 == 1 ? 1 : -1;
        byArray[0][0] = this->tileAt(n - n1, n2);
        byArray[1][0] = this->tileAt(n, n2);
        byArray[2][0] = this->tileAt(n + n1, n2);
        int32_t n4 = n2 - n1;
        int32_t n5 = 0;
        while (n5 < 5) {
            byArray[n5][1] = this->tileAt(n + (n5 - 2) * n1, n4);
            ++n5;
        }
        n4 = n2 - 2 * n1;
        int32_t n6 = 0;
        while (n6 < 7) {
            byArray[n6][2] = this->tileAt(n + (n6 - 3) * n1, n4);
            ++n6;
        }
        n4 = n2 - 3 * n1;
        int32_t n7 = 0;
        while (n7 < 9) {
            byArray[n7][3] = this->tileAt(n + (n7 - 4) * n1, n4);
            ++n7;
        }
        n4 = n2 - 4 * n1;
        int32_t n8 = 0;
        while (n8 < 9) {
            byArray[n8][4] = this->tileAt(n + (n8 - 4) * n1, n4);
            ++n8;
        }
        return;
    }
    if (n3 != 2 && n3 != 4) return;
    int32_t n9 = 0;
    n9 = n3 == 2 ? 1 : -1;
    byArray[0][0] = this->tileAt(n, n2 - n9);
    byArray[1][0] = 0;
    byArray[2][0] = this->tileAt(n, n2 + n9);
    int32_t n10 = n + n9;
    int32_t n11 = 0;
    while (n11 < 5) {
        byArray[n11][1] = this->tileAt(n10, n2 + (n11 - 2) * n9);
        ++n11;
    }
    n10 = n + 2 * n9;
    int32_t n12 = 0;
    while (n12 < 7) {
        byArray[n12][2] = this->tileAt(n10, n2 + (n12 - 3) * n9);
        ++n12;
    }
    n10 = n + 3 * n9;
    int32_t n13 = 0;
    while (n13 < 9) {
        byArray[n13][3] = this->tileAt(n10, n2 + (n13 - 4) * n9);
        ++n13;
    }
    n10 = n + 4 * n9;
    int32_t n14 = 0;
    while (n14 < 9) {
        byArray[n14][4] = this->tileAt(n10, n2 + (n14 - 4) * n9);
        ++n14;
    }
}

void Dungeon::scanSurroundings(int32_t x, int32_t y, int32_t facing,
                               SharedArray<SharedArray<int8_t>> into) {
    this->b(x, y, facing, into);
}

void Dungeon::c(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray) {
    this->a(n, n2, n3, 7, byArray);
}

void Dungeon::a(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray) {
    this->a(n, n2, n3, 17, byArray);
}

void Dungeon::a(int32_t n, int32_t n2, int32_t n3, int32_t n4, SharedArray<SharedArray<int8_t>> byArray) {
    int32_t n1;
    int32_t n5 = n4 / 2;
    if (n3 == 1 || n3 == 3) {
        int32_t n6;
        int32_t n7 = 0;
        n7 = n3 == 1 ? 1 : -1;
        int32_t n8 = 0;
        while (n8 < n4) {
            n6 = 0;
            while (n6 < n4) {
                byArray[n6][n8] = (int8_t)(this->tileAt(n + (n6 - n5) * n7, n2 + (n8 - n5) * n7) & 1);
                if ((byArray[n6][n8] & 1) == 0) {
                    byArray[n6][n8] =
                        (int8_t)(this->tileAt(n + (n6 - n5) * n7, n2 + (n8 - n5) * n7) & 8);
                }
                ++n6;
            }
            ++n8;
        }
        if (this->id_ > 1) {
            int32_t n9;
            int32_t n10;
            int32_t n11;
            Monster decodedMonster;
            for (const SharedArray<int8_t> &record :
                 worldState_.monsters.at((std::size_t)(this->id_ - 1))) {
                Monster *monster = Monster::fromRecord(&decodedMonster, record, this);
                n11 = n7 * (monster->gridX_ - n) + n5;
                n10 = n7 * (monster->gridY_ - n2) + n5;
                if (n11 < 0 || n11 >= n4 || n10 < 0 || n10 >= n4 || !monster->seen_) continue;
                SharedArray<int8_t> byArray1 = byArray[n11];
                int32_t n12 = n10;
                byArray1[n12] = (int8_t)(byArray1[n12] | 2);
            }
            for (const SharedArray<int8_t> &chest :
                 worldState_.chests.at((std::size_t)(this->id_ - 1))) {
                n11 = n7 * (chest[0] - n) + n5;
                n10 = n7 * (chest[1] - n2) + n5;
                n9 = 1;
                if (n11 < 0 || n11 >= n4 || n10 < 0 || n10 >= n4 || n9 == 0) continue;
                SharedArray<int8_t> byArray2 = byArray[n11];
                int32_t n13 = n10;
                byArray2[n13] = (int8_t)(byArray2[n13] | 4);
            }
            for (const SharedArray<int8_t> &byArray3 :
                 worldState_.droppedItems.at((std::size_t)(this->id_ - 1))) {
                bool bl1;
                n10 = n7 * (byArray3[0] - n) + n5;
                n9 = n7 * (byArray3[1] - n2) + n5;
                bool bl2 = bl1 = (byArray3[6] & 1) != 0;
                (void)bl2;
                if (n10 < 0 || n10 >= n4 || n9 < 0 || n9 >= n4 || !bl1) continue;
                SharedArray<int8_t> byArray4 = byArray[n10];
                int32_t n14 = n9;
                byArray4[n14] = (int8_t)(byArray4[n14] | 4);
            }
        } else {
            n6 = 0;
            while (n6 < 7) {
                if (n6 != 6 || worldState_.npcs.wardenPresent) {
                    if (worldState_.npcs.npcPresent[n6]) {
                        int32_t n15 = n7 * (NpcSystem::npcGridX_[n6] - n) + n5;
                        int32_t n16 = n7 * (NpcSystem::npcGridY_[n6] - n2) + n5;
                        bool bl3 = true;
                        if (n15 >= 0 && n15 < n4 && n16 >= 0 && n16 < n4 && bl3) {
                            SharedArray<int8_t> byArray5 = byArray[n15];
                            int32_t n17 = n16;
                            byArray5[n17] = (int8_t)(byArray5[n17] | 4);
                        }
                    }
                    ++n6;
                    continue;
                }
                return;
            }
        }
        return;
    }
    if (n3 != 2 && n3 != 4) return;
    int32_t n18 = 0;
    n18 = n3 == 2 ? 1 : -1;
    int32_t n19 = 0;
    while (n19 < n4) {
        n1 = 0;
        while (n1 < n4) {
            byArray[n1][n19] = (int8_t)(this->tileAt(n - (n19 - n5) * n18, n2 + (n1 - n5) * n18) & 1);
            if ((byArray[n1][n19] & 1) == 0) {
                byArray[n1][n19] =
                    (int8_t)(this->tileAt(n - (n19 - n5) * n18, n2 + (n1 - n5) * n18) & 8);
            }
            ++n1;
        }
        ++n19;
    }
    if (this->id_ > 1) {
        int32_t n20;
        int32_t n21;
        int32_t n22;
        Monster decodedMonster;
        for (const SharedArray<int8_t> &record :
             worldState_.monsters.at((std::size_t)(this->id_ - 1))) {
            Monster *monster = Monster::fromRecord(&decodedMonster, record, this);
            n22 = n18 * (monster->gridY_ - n2) + n5;
            n21 = n5 - n18 * (monster->gridX_ - n);
            if (n22 < 0 || n22 >= n4 || n21 < 0 || n21 >= n4 || !monster->seen_) continue;
            SharedArray<int8_t> byArray6 = byArray[n22];
            int32_t n23 = n21;
            byArray6[n23] = (int8_t)(byArray6[n23] | 2);
        }
        for (const SharedArray<int8_t> &chest :
             worldState_.chests.at((std::size_t)(this->id_ - 1))) {
            n22 = n18 * (chest[1] - n2) + n5;
            n21 = n5 - n18 * (chest[0] - n);
            n20 = 1;
            if (n22 < 0 || n22 >= n4 || n21 < 0 || n21 >= n4 || n20 == 0) continue;
            SharedArray<int8_t> byArray7 = byArray[n22];
            int32_t n24 = n21;
            byArray7[n24] = (int8_t)(byArray7[n24] | 4);
        }
        for (const SharedArray<int8_t> &byArray8 :
             worldState_.droppedItems.at((std::size_t)(this->id_ - 1))) {
            bool bl4;
            n21 = n18 * (byArray8[1] - n2) + n5;
            n20 = n5 - n18 * (byArray8[0] - n);
            bool bl5 = bl4 = (byArray8[6] & 1) != 0;
            (void)bl5;
            if (n21 < 0 || n21 >= n4 || n20 < 0 || n20 >= n4 || !bl4) continue;
            SharedArray<int8_t> byArray9 = byArray[n21];
            int32_t n25 = n20;
            byArray9[n25] = (int8_t)(byArray9[n25] | 4);
        }
    } else {
        n1 = 0;
        while (n1 < 7) {
            if (n1 != 6 || worldState_.npcs.wardenPresent) {
                if (worldState_.npcs.npcPresent[n1]) {
                    int32_t n26 = n18 * (NpcSystem::npcGridY_[n1] - n2) + n5;
                    int32_t n27 = n5 - n18 * (NpcSystem::npcGridX_[n1] - n);
                    bool bl6 = true;
                    if (n26 >= 0 && n26 < n4 && n27 >= 0 && n27 < n4 && bl6) {
                        SharedArray<int8_t> byArray10 = byArray[n26];
                        int32_t n28 = n27;
                        byArray10[n28] = (int8_t)(byArray10[n28] | 4);
                    }
                }
                ++n1;
                continue;
            }
            break;
        }
    }
}

SharedArray<int32_t> Dungeon::screenSlotOf(int32_t n, int32_t n2, int32_t n3, int32_t n4, int32_t n5) {
    int32_t n1 = 0;
    int32_t n6 = 0;
    if (n3 == 1 || n3 == 3) {
        int32_t n7 = 0;
        n7 = n3 == 1 ? 1 : -1;
        n1 = n7 * (n4 - n) + 3;
        n6 = n7 * (n5 - n2) + 3;
    } else if (n3 == 2 || n3 == 4) {
        int32_t n8 = 0;
        n8 = n3 == 2 ? 1 : -1;
        n1 = n8 * (n5 - n2) + 3;
        n6 = 3 - n8 * (n4 - n);
    }
    this->slotScratch_[0] = n1;
    this->slotScratch_[1] = n6;
    return this->slotScratch_;
}

int8_t Dungeon::tileAt(int32_t n, int32_t n2) {
    int32_t n1 = n;
    int32_t n3 = n2;
    int8_t by1 = this->id_;
    DungeonCore *i2 = nullptr;
    if (n < 0) {
        by1 = this->geometry_[3];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        if (by1 == 1 || this->id_ == 1) {
            n1 = (int8_t)(i2->width_ - 1);
            n3 = (int8_t)(n3 + (i2->height_ - this->height_) / 2);
        } else {
            n1 = (int8_t)(i2->width_ - 1);
        }
    } else if (n >= this->width_) {
        by1 = this->geometry_[1];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        if (by1 == 1 || this->id_ == 1) {
            n1 = 0;
            n3 = (int8_t)(n3 + (i2->height_ - this->height_) / 2);
        } else {
            n1 = 0;
        }
    } else if (n2 < 0) {
        by1 = this->geometry_[0];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        if (by1 == 1 || this->id_ == 1) {
            n1 = (int8_t)(n1 + (i2->width_ - this->width_) / 2);
            n3 = (int8_t)(i2->height_ - 1);
        } else {
            n3 = (int8_t)(i2->height_ - 1);
        }
    } else if (n2 >= this->height_) {
        by1 = this->geometry_[2];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        if (by1 == 1 || this->id_ == 1) {
            n1 = (int8_t)(n1 + (i2->width_ - this->width_) / 2);
            n3 = 0;
        } else {
            n3 = 0;
        }
    }
    if (by1 != this->id_) {
        if (n1 < 0 || n1 >= i2->width_) {
            return 1;
        }
        if (n3 < 0 || n3 >= i2->height_) {
            return 1;
        }
        if (i2->populated_) {
            return i2->tiles_[n1][n3];
        }
        return 1;
    }
    return this->tiles_[n][n2];
}

int8_t Dungeon::a(int32_t n, int32_t n2, SharedArray<SharedArray<int8_t>> byArray) {
    if (n2 < 4) {
        return byArray[n + n2 + 1][n2];
    }
    return byArray[n + n2][n2];
}

SharedArray<std::string> Dungeon::name() {
    return dungeonNames_[this->id_ - 1];
}

void Dungeon::loadNames(platform::PlatformContext *context) {
    BinaryReader *dataInputStream =
        GameUtil::openResource(context, std::string("/dungnamesin.dat"));
    dungeonNames_ = SharedArray<SharedArray<std::string>>(37);
    int32_t n1 = 0;
    while (n1 < 37) {
        Dungeon::dungeonNames_[n1] = SharedArray<std::string>(2);
        int32_t n2 = 0;
        while (n2 < 2) {
            Dungeon::dungeonNames_[n1][n2] = dataInputStream->readUTF();
            ++n2;
        }
        ++n1;
    }
    delete dataInputStream;
}

void Dungeon::refreshMonsterBits(int32_t n, int32_t n2) {
    int32_t n1 = max32(n - 4, 0);
    int32_t n3 = min32(n + 4, this->width_ - 1);
    int32_t n4 = max32(n2 - 4, 0);
    int32_t n5 = min32(n2 + 4, this->height_ - 1);
    int32_t n6 = n1;
    while (n6 <= n3) {
        int32_t n7 = n4;
        while (n7 <= n5) {
            int8_t by1 = this->tiles_[n6][n7];
            if (!GameUtil::hasFlag((int8_t)1, by1)) {
                this->tiles_[n6][n7] = GameUtil::clearFlag((int8_t)2, by1);
            }
            ++n7;
        }
        ++n6;
    }
    const std::size_t dungeonIndex = (std::size_t)(this->id_ - 1);
    if (worldState_.monsters.hasTable(dungeonIndex)) {
        for (const SharedArray<int8_t> &byArray1 : worldState_.monsters.at(dungeonIndex)) {
            int8_t by2 = byArray1[4];
            int8_t by3 = byArray1[5];
            if (by2 < n1 || by2 > n3 || by3 < n4 || by3 > n5) continue;
            SharedArray<int8_t> byArray2 = this->tiles_[by2];
            int8_t by4 = by3;
            byArray2[by4] = (int8_t)(byArray2[by4] | 2);
        }
    }
}

void Dungeon::initializeStatics() {
}

}
