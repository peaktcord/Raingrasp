#include "src/dawnstar/dungeon.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/monster.hpp"
#include "src/dawnstar/npc_script.hpp"
#include "src/common/game/player.hpp"

namespace dawnstar {

SharedArray<std::string> Dungeon::dungeonNames_;
void Dungeon::initializeStatics() {
    dungeonNames_ = SharedArray<std::string>{
        "Dawnstar",        "North Creek",      "North Creek 2",   "North Creek 3",
        "Ice Spike",       "Ice Spike 2",      "Ice Spike 3",     "Blind Fjord",
        "Blind Fjord 2",   "Blind Fjord 3",    "Slipneck Fjord",  "Slipneck Fjord 2",
        "Slipneck Fjord 3","Troll Pace",       "Troll Pace 2",    "Troll Pace 3",
        "Ice Tribe Haven", "Ice Tribe Haven 2","Ice Tribe Haven 3","Dawnstar Run",
        "Dawnstar Run 2",  "Dawnstar Run 3",   "Massacre Caves",  "Massacre Caves 2",
        "Massacre Caves 3","Frostheim",        "Frostheim 2",     "Frostheim 3",
        "Glacier Run",     "Glacier Run 2",    "Glacier Run 3",   "Troll Hole",
        "Troll Hole 2",    "Troll Hole 3",     "Ice Council",     "Ice Council 2",
        "Ice Council 3"};
}

Dungeon::Dungeon(worldstate::DungeonRegistry &dungeons,
                 worldstate::WorldState &worldState)
    : DungeonCore(worldState, true), dungeons_(dungeons) {}

Dungeon::Dungeon(worldstate::DungeonRegistry &dungeons,
                 worldstate::WorldState &worldState, int8_t by, SharedArray<int8_t> byArray)
    : Dungeon(dungeons, worldState) {
    this->id_ = by;
    this->assignLevel();
    this->width_ = (int16_t)35;
    this->height_ = (int16_t)35;
    this->geometry_ = byArray;
    this->exitDir1_ = this->geometry_[4];
    this->exitDir2_ = this->geometry_[5];
}

Dungeon::Dungeon(worldstate::DungeonRegistry &dungeons,
                 worldstate::WorldState &worldState, int8_t by, SharedArray<int8_t> byArray, int32_t n,
                 int32_t n2, SharedArray<SharedArray<int8_t>> byArray2)
    : Dungeon(dungeons, worldState) {
    this->id_ = by;
    this->assignLevel();
    this->width_ = (int16_t)n;
    this->height_ = (int16_t)n2;
    this->tiles_ = byArray2;
    this->geometry_ = byArray;
    this->exitDir1_ = this->geometry_[4];
    this->exitDir2_ = this->geometry_[5];
    int32_t n1 = 0;
    while (n1 < 5) {
        SharedArray<int8_t> byArray1 = this->tiles_[NpcSystem::npcGridX_[n1]];
        int8_t by1 = NpcSystem::npcGridY_[n1];
        byArray1[by1] = (int8_t)(byArray1[by1] | 0x20);
        ++n1;
    }
    this->populated_ = true;
}

void Dungeon::spawnMonsters(int32_t n) {
    int32_t n1 = 0;
    while (n1 < n) {
        while (!this->spawnNear(wrappingAbs(worldState_.random->nextInt() % this->width_),
                                wrappingAbs(worldState_.random->nextInt() % this->height_), -1)) {
        }
        ++n1;
    }
}

bool Dungeon::spawnNear(int32_t n, int32_t n2, int32_t n3) {
    int32_t n1 = -1;
    if (n3 == 42 || n3 == 41) {
        n1 = n3;
        n3 = this->level_;
    }
    if (n3 < 0) {
        n3 = this->level_;
    }
    bool bl1 = false;
    int32_t n4 = 0;
    while (n4 <= 4) {
        int32_t n5 = n;
        int32_t n6 = n2;
        if (n4 < 2) {
            n5 += 2 * n4 - 1;
        } else {
            n6 += 2 * n4 - 5;
        }
        if (this->isFree(n5, n6)) {
            Monster *d2 = Monster::spawn(worldState_.random, n3, this, n1);
            bl1 = true;
            d2->gridX_ = (int8_t)n5;
            d2->gridY_ = (int8_t)n6;
            d2->store();
            this->tiles_[d2->gridX_][d2->gridY_] = GameUtil::setFlag((int8_t)2, this->tiles_[d2->gridX_][d2->gridY_]);
            break;
        }
        ++n4;
    }
    return bl1;
}

void Dungeon::rebuildOccupancy() {
    int32_t n1;
    int32_t n2 = 0;
    while (n2 < this->width_) {
        n1 = 0;
        while (n1 < this->height_) {
            this->tiles_[n2][n1] = GameUtil::clearFlag((int8_t)2, this->tiles_[n2][n1]);
            this->tiles_[n2][n1] = GameUtil::clearFlag((int8_t)16, this->tiles_[n2][n1]);
            this->tiles_[n2][n1] = GameUtil::clearFlag((int8_t)4, this->tiles_[n2][n1]);
            ++n1;
        }
        ++n2;
    }
    for (const SharedArray<int8_t> &monster :
         worldState_.monsters.at((std::size_t)(this->id_ - 1))) {
        SharedArray<int8_t> byArray1 = this->tiles_[monster[4]];
        int32_t n6 = monster[5];
        byArray1[n6] = (int8_t)(byArray1[n6] | 2);
    }
    const std::size_t dungeonIndex = (std::size_t)(this->id_ - 1);
    if (worldState_.chests.hasTable(dungeonIndex)) {
        for (const SharedArray<int8_t> &byArray2 : worldState_.chests.at(dungeonIndex)) {
            SharedArray<int8_t> byArray3 = this->tiles_[byArray2[0]];
            int8_t by1 = byArray2[1];
            byArray3[by1] = (int8_t)(byArray3[by1] | 0x10);
        }
    }
    for (const SharedArray<int8_t> &byArray4 :
         worldState_.droppedItems.at((std::size_t)(this->id_ - 1))) {
        SharedArray<int8_t> byArray5 = this->tiles_[byArray4[0]];
        int8_t by2 = byArray4[1];
        byArray5[by2] = (int8_t)(byArray5[by2] | 4);
    }
}

int8_t Dungeon::runMonsters(int64_t l, Player *j2) {
    int8_t by1;
    int8_t by2 = j2->gridX_;
    int8_t by3 = j2->gridY_;
    bool bl1 = false;
    bool bl2 = false;
    Monster *d2 = new Monster();
    SharedArray<int8_t> var1;
    int32_t n1 = -3;
    while (n1 < 4) {
        if (by2 + n1 >= 0 && by2 + n1 < this->width_) {
            int32_t n2 = -3;
            while (n2 < 4) {
                int32_t n3;
                if (by3 + n2 >= 0 && by3 + n2 < this->height_ &&
                    (n3 = (n1 >= 0 ? n1 : -n1) + (n2 >= 0 ? n2 : -n2)) != 0 && n3 <= 3 &&
                    GameUtil::hasFlag((int8_t)2, by1 = this->tiles_[by2 + n1][by3 + n2]) &&
                    !(var1 = worldState_.monsters.findAt(
                          (std::size_t)(this->id_ - 1), by2 + n1, by3 + n2))
                         .isNull()) {
                    Monster::fromRecord(d2, var1, this);
                    if (n3 == 1) {
                        if (d2->attack(j2, l)) {
                            bl1 = true;
                        }
                        d2->store();
                    } else if (d2->takeTurn(by2, by3)) {
                        bl2 = true;
                    }
                }
                ++n2;
            }
        }
        ++n1;
    }
    by1 = 0;
    if (bl1) {
        by1 = (int8_t)(by1 + 2);
    }
    if (bl2) {
        by1 = (int8_t)(by1 + 1);
    }
    return by1;
}

void Dungeon::removeDroppedItem(SharedArray<int8_t> byArray) {
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
}

void Dungeon::clearItemBit(int32_t n, int32_t n2) {
    this->tiles_[n][n2] = GameUtil::clearFlag((int8_t)4, this->tiles_[n][n2]);
}

void Dungeon::a(int32_t n, int32_t n2, int32_t n3, SharedArray<SharedArray<int8_t>> byArray) {
    if (n3 == 1 || n3 == 3) {
        int32_t n1 = -1;
        if (n3 == 1) {
            n1 = 1;
        }
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
    if (n3 == 2 || n3 == 4) {
        int32_t n9 = -1;
        if (n3 == 2) {
            n9 = 1;
        }
        byArray[0][0] = this->tileAt(n, n2 - n9);
        byArray[1][0] = this->tileAt(n, n2);
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
}

void Dungeon::a(int32_t n, int32_t n2, int32_t n3, int32_t n4, SharedArray<SharedArray<int8_t>> byArray) {
    int32_t n1 = n4 / 2;
    Monster *d2 = new Monster();
    if (n3 == 1 || n3 == 3) {
        int32_t n5 = 0;
        n5 = n3 == 1 ? 1 : -1;
        int32_t n6 = 0;
        while (n6 < n4) {
            int32_t n7 = 0;
            while (n7 < n4) {
                int8_t by1 = this->tileAt(n + (n7 - n1) * n5, n2 + (n6 - n1) * n5);
                byArray[n7][n6] = (int8_t)(by1 & 1);
                if ((byArray[n7][n6] & 1) == 0) {
                    SharedArray<int8_t> byArray1;
                    if ((by1 & 4) != 0 || (by1 & 0x10) != 0 || (by1 & 0x20) != 0) {
                        SharedArray<int8_t> byArray2 = byArray[n7];
                        int32_t n8 = n6;
                        byArray2[n8] = (int8_t)(byArray2[n8] | 4);
                    } else {
                        byArray[n7][n6] = (int8_t)(by1 & 8);
                    }
                    if ((by1 & 2) != 0 &&
                        !(byArray1 = worldState_.monsters.findAt(
                              (std::size_t)(this->id_ - 1), n + (n7 - n1) * n5,
                              n2 + (n6 - n1) * n5))
                             .isNull()) {
                        Monster::fromRecord(d2, byArray1, this);
                        if (d2->seen_) {
                            SharedArray<int8_t> byArray3 = byArray[n7];
                            int32_t n9 = n6;
                            byArray3[n9] = (int8_t)(byArray3[n9] | 2);
                        }
                    }
                }
                ++n7;
            }
            ++n6;
        }
        return;
    }
    if (n3 == 2 || n3 == 4) {
        int32_t n10 = 0;
        n10 = n3 == 2 ? 1 : -1;
        int32_t n11 = 0;
        while (n11 < n4) {
            int32_t n12 = 0;
            while (n12 < n4) {
                int8_t by2 = this->tileAt(n - (n11 - n1) * n10, n2 + (n12 - n1) * n10);
                byArray[n12][n11] = (int8_t)(by2 & 1);
                if ((byArray[n12][n11] & 1) == 0) {
                    SharedArray<int8_t> byArray4;
                    if ((by2 & 4) != 0 || (by2 & 0x10) != 0 || (by2 & 0x20) != 0) {
                        SharedArray<int8_t> byArray5 = byArray[n12];
                        int32_t n13 = n11;
                        byArray5[n13] = (int8_t)(byArray5[n13] | 4);
                    } else {
                        byArray[n12][n11] = (int8_t)(by2 & 8);
                    }
                    if ((by2 & 2) != 0 &&
                        !(byArray4 = worldState_.monsters.findAt(
                              (std::size_t)(this->id_ - 1), n - (n11 - n1) * n10,
                              n2 + (n12 - n1) * n10))
                             .isNull()) {
                        Monster::fromRecord(d2, byArray4, this);
                        if (d2->seen_) {
                            SharedArray<int8_t> byArray6 = byArray[n12];
                            int32_t n14 = n11;
                            byArray6[n14] = (int8_t)(byArray6[n14] | 2);
                        }
                    }
                }
                ++n12;
            }
            ++n11;
        }
    }
}

void Dungeon::scanSurroundings(int32_t x, int32_t y, int32_t facing,
                               SharedArray<SharedArray<int8_t>> into) {
    this->a(x, y, facing, into);
}

Monster *Dungeon::monsterAt(Monster *into, int32_t x, int32_t y) {
    SharedArray<int8_t> record = worldState_.monsters.findAt((std::size_t)(this->id_ - 1), x, y);
    if (record.isNull()) {
        return nullptr;
    }
    return Monster::fromRecord(into, record, this);
}

int8_t Dungeon::tileAt(int32_t n, int32_t n2) {
    int8_t by1 = this->id_;
    bool bl1 = false;
    DungeonCore *i2 = nullptr;
    if (n < 0) {
        by1 = this->geometry_[3];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        n = (int8_t)(i2->width_ + n);
        if (by1 == 1 || this->id_ == 1) {
            n2 = (int8_t)(n2 + (i2->height_ - this->height_) / 2);
            if (n == i2->width_ - 2) {
                bl1 = true;
            }
        }
    } else if (n >= this->width_) {
        by1 = this->geometry_[1];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        n -= this->width_;
        if (by1 == 1 || this->id_ == 1) {
            n2 = (int8_t)(n2 + (i2->height_ - this->height_) / 2);
            if (n == 1) {
                bl1 = true;
            }
        }
    } else if (n2 < 0) {
        by1 = this->geometry_[0];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        n2 = (int8_t)(i2->height_ + n2);
        if (by1 == 1 || this->id_ == 1) {
            n = (int8_t)(n + (i2->width_ - this->width_) / 2);
            if (n2 == i2->height_ - 2) {
                bl1 = true;
            }
        }
    } else if (n2 >= this->height_) {
        by1 = this->geometry_[2];
        if (by1 <= 0) {
            return 1;
        }
        i2 = dungeons_.atId(by1);
        n2 -= this->height_;
        if (by1 == 1 || this->id_ == 1) {
            n = (int8_t)(n + (i2->width_ - this->width_) / 2);
            if (n2 == 1) {
                bl1 = true;
            }
        }
    }
    if (by1 != this->id_) {
        if (n < 0 || n >= i2->width_) {
            return 1;
        }
        if (n2 < 0 || n2 >= i2->height_) {
            return 1;
        }
        if (bl1 && i2->tiles_[n][n2] == 0) {
            return 64;
        }
        if (i2->populated_) {
            return i2->tiles_[n][n2];
        }
        return 1;
    }
    return this->tiles_[n][n2];
}

std::string Dungeon::name() {
    return dungeonNames_[this->id_ - 1];
}

}
