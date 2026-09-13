#include "src/dawnstar/dungeon_gen.hpp"
#include "src/common/game/items.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/game/util.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/monster.hpp"
#include "src/dawnstar/npc_script.hpp"

namespace dawnstar {

const int32_t DungeonGen::kChestTier[36] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 3, 1, 3, 3,
                        1, 3, 3, 3, 3, 5, 3, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5};

DungeonGen::DungeonGen(worldstate::DungeonRegistry &dungeons, UIWidget *h2,
                       worldstate::WorldState &worldState) {
    platformContext_ = worldState.platformContext;
    this->buildCampGrid();
    try {
        this->loadGeometry();
    } catch (const std::exception &exception) {
        // Generation continues on empty geometry rather than aborting the
        // build, so without this line the resulting broken dungeons are the
        // only symptom a report would carry.
        platform::writeLogLine(std::string("ERROR: failed to load dungeon geometry: ") +
                               exception.what());
    }
    dungeons.emplace<Dungeon>(0, dungeons, worldState, 1, this->geometry_[0], campWidth_,
                     campHeight_, campGrid_);
    int32_t n1 = 1;
    while (n1 < 37) {
        Dungeon *dungeon = dungeons.emplace<Dungeon>((std::size_t)n1, dungeons, worldState,
                                            (int8_t)(n1 + 1), this->geometry_[n1]);
        this->generate(dungeon);
        h2->progressPercent_ = 60 + n1;
        ++n1;
    }
}

void DungeonGen::buildCampGrid() {
    int32_t n1;
    campWidth_ = 19;
    campHeight_ = 19;
    campGrid_ = makeSharedArray2D<int8_t>(campHeight_, campWidth_);
    int32_t n2 = 0;
    while (n2 < campHeight_) {
        n1 = 0;
        while (n1 < campWidth_) {
            campGrid_[n2][n1] = 1;
            ++n1;
        }
        ++n2;
    }
    n1 = 0;
    while (n1 < campWidth_) {
        campGrid_[n1][9] = 0;
        ++n1;
    }
    int32_t n3 = 0;
    while (n3 < campHeight_) {
        campGrid_[9][n3] = 0;
        ++n3;
    }
    int32_t n4 = 4;
    while (n4 < 15) {
        campGrid_[4][n4] = 0;
        campGrid_[14][n4] = 0;
        ++n4;
    }
    int32_t n5 = 4;
    while (n5 < 15) {
        campGrid_[n5][4] = 0;
        campGrid_[n5][14] = 0;
        ++n5;
    }
    campGrid_[5][6] = 0;
    campGrid_[6][6] = 0;
    campGrid_[7][6] = 0;
    campGrid_[7][5] = 0;
    campGrid_[7][7] = 0;
    campGrid_[12][6] = 0;
    campGrid_[13][6] = 0;
    campGrid_[12][8] = 0;
    campGrid_[8][8] = 0;
    campGrid_[10][8] = 0;
    campGrid_[8][10] = 0;
    campGrid_[10][10] = 0;
    campGrid_[11][12] = 0;
    campGrid_[11][13] = 0;
    campGrid_[12][12] = 0;
    campGrid_[5][11] = 0;
    campGrid_[6][11] = 0;
}

void DungeonGen::loadGeometry() {
    BinaryReader *dataInputStream =
        GameUtil::openDatFile(platformContext_, std::string("geomin.dat"));
    this->geometry_ = makeSharedArray2D<int8_t>(37, 6);
    int32_t n1 = 0;
    while (n1 < 37) {
        int32_t n2 = 0;
        while (n2 < 6) {
            this->geometry_[n1][n2] = dataInputStream->readByte();
            ++n2;
        }
        ++n1;
    }
    delete dataInputStream;
}

void DungeonGen::generate(Dungeon *i2) {
    int32_t n1;
    int32_t n2;
    int32_t n3;
    int32_t n4;
    int32_t n5;
    int32_t n6;
    int64_t l1 = (int64_t)i2->id_ * 8000;
    i2->tiles_ = makeSharedArray2D<int8_t>(i2->width_, i2->height_);
    i2->exitDir1_ = i2->geometry_[4];
    i2->exitDir2_ = i2->geometry_[5];
    this->rooms_.clear();
    int32_t n7 = -1;
    this->rng_ = new GameRandom(l1);
    int32_t n8 = 0;
    while (n8 < 35) {
        n6 = 0;
        while (n6 < 35) {
            i2->tiles_[n8][n6] = 1;
            ++n6;
        }
        ++n8;
    }
    this->roomKeys_.clear();
    this->unlinkedRooms_.clear();
    if (this->isExitDirection((int16_t)i2->exitDir1_) && (n7 = this->carveEntrance(i2, (int16_t)i2->exitDir1_)) >= 0) {
        this->addRoomKey(n7);
    }
    if (this->isExitDirection((int16_t)i2->exitDir2_) && (n7 = this->carveEntrance(i2, (int16_t)i2->exitDir2_)) >= 0) {
        this->addRoomKey(n7);
    }
    n6 = 0;
    int32_t n9 = 0;
    while (n9 < 15) {
        SharedArray<int16_t> sArray1 = this->rollRoom();
        if (!this->tryPlaceRoom(sArray1, i2)) continue;
        n5 = this->packKey((int16_t)sArray1[4], (int16_t)sArray1[5]);
        this->addRoomKey(n5);
        if (++n9 < 2 || n6 != 0) continue;
        int16_t s1 = sArray1[4];
        n4 = sArray1[5];
        int16_t s2 = (int16_t)(sArray1[2] - sArray1[0] + 1);
        int16_t s3 = (int16_t)(sArray1[3] - sArray1[1] + 1);
        if (s2 < 3 || s3 < 3) continue;
        n3 = (int16_t)(sArray1[0] + s2 / 2);
        n2 = (int16_t)(sArray1[1] + s3 / 2);
        if (n3 == s1 && n2 == n4) continue;
        if (i2->id_ == 3) {
            SharedArray<int8_t> byArray1 = i2->tiles_[n3];
            int32_t n10 = n2;
            byArray1[n10] = (int8_t)(byArray1[n10] | 0x20);
            NpcSystem::npcGridX_[5] = (int8_t)n3;
            NpcSystem::npcGridY_[5] = (int8_t)n2;
        } else if (i2->id_ == 12) {
            SharedArray<int8_t> byArray2 = i2->tiles_[n3];
            int32_t n11 = n2;
            byArray2[n11] = (int8_t)(byArray2[n11] | 0x20);
            NpcSystem::npcGridX_[6] = (int8_t)n3;
            NpcSystem::npcGridY_[6] = (int8_t)n2;
        } else if (i2->id_ == 21) {
            SharedArray<int8_t> byArray3 = i2->tiles_[n3];
            int32_t n12 = n2;
            byArray3[n12] = (int8_t)(byArray3[n12] | 0x20);
            NpcSystem::npcGridX_[7] = (int8_t)n3;
            NpcSystem::npcGridY_[7] = (int8_t)n2;
        } else if (i2->id_ == 30) {
            SharedArray<int8_t> byArray4 = i2->tiles_[n3];
            int32_t n13 = n2;
            byArray4[n13] = (int8_t)(byArray4[n13] | 0x20);
            NpcSystem::npcGridX_[8] = (int8_t)n3;
            NpcSystem::npcGridY_[8] = (int8_t)n2;
        } else {
            SharedArray<int8_t> byArray5 = i2->tiles_[n3];
            int32_t n14 = n2;
            byArray5[n14] = (int8_t)(byArray5[n14] | 8);
        }
        n6 = 1;
    }
    this->linkRooms(i2);
    int32_t n15 = (int32_t)this->rooms_.size();
    n5 = i2->id_ << 8;
    (void)n5;
    Monster *d2 = nullptr;
    n4 = 0;
    while (n4 < n15) {
        SharedArray<int16_t> sArray2 = this->rooms_[(size_t)n4];
        d2 = Monster::spawn(this->rng_, i2->level_, i2, -1);
        d2->gridX_ = (int8_t)sArray2[4];
        d2->gridY_ = (int8_t)sArray2[5];
        SharedArray<int8_t> byArray6 = i2->tiles_[d2->gridX_];
        int8_t by1 = d2->gridY_;
        byArray6[by1] = (int8_t)(byArray6[by1] | 2);
        d2->store();
        ++n4;
    }
    n15 = (int32_t)this->rooms_.size();
    SharedArray<int32_t> nArray1(n15);
    SharedArray<int32_t> nArray2(n15);
    n3 = 0;
    while (n3 < n15) {
        nArray1[n3] = n3;
        nArray2[n3] = n2 = GameUtil::randomInt(this->rng_, 1000);
        ++n3;
    }
    n2 = 1;
    while (n2 < n15) {
        int32_t n16 = nArray2[n2];
        n1 = nArray1[n2];
        int32_t n17 = n2 - 1;
        while (n17 >= 0 && nArray2[n17] < n16) {
            nArray2[n17 + 1] = nArray2[n17];
            nArray1[n17 + 1] = nArray1[n17];
            --n17;
        }
        nArray2[n17 + 1] = n16;
        nArray1[n17 + 1] = n1;
        ++n2;
    }
    SharedArray<int32_t> nArray3(5);
    n1 = 0;
    while (n1 < 5) {
        nArray3[n1] = nArray1[n1];
        ++n1;
    }
    this->placeChests(nArray3, i2);
    this->rooms_.clear();
    this->roomKeys_.clear();
    this->unlinkedRooms_.clear();
}

void DungeonGen::placeChests(SharedArray<int32_t> nArray, Dungeon *i2) {
    int32_t n1 = 0;
    int32_t n2 = kChestTier[i2->level_ - 1];
    bool bl1 = true;
    int32_t n3 = 0;
    while (n3 < 5) {
        int32_t n4 = nArray[n3];
        SharedArray<int16_t> sArray1 = this->rooms_[(size_t)n4];
        if (bl1) {
            n1 = Items::randomOfTier(this->rng_, n2);
            bl1 = false;
        } else {
            n1 = Items::randomDropped(this->rng_, i2->level_, 2);
        }
        int16_t s1 = (int16_t)(sArray1[2] - sArray1[0] + 1);
        int16_t s2 = (int16_t)(sArray1[3] - sArray1[1] + 1);
        int16_t s3 = (int16_t)(sArray1[0] + wrappingAbs(this->rng_->nextInt() % s1));
        int16_t s4 = (int16_t)(sArray1[1] + wrappingAbs(this->rng_->nextInt() % s2));
        while ((i2->tiles_[s3][s4] & 8) != 0 && (i2->tiles_[s3][s4] & 0x20) != 0) {
            s3 = (int16_t)(sArray1[0] + wrappingAbs(this->rng_->nextInt() % s1));
        s4 = (int16_t)(sArray1[1] + wrappingAbs(this->rng_->nextInt() % s2));
        }
        SharedArray<int8_t> byArray1(8);
        byArray1[0] = (int8_t)s3;
        byArray1[1] = (int8_t)s4;
        byArray1[2] = bl1 ? (int8_t)1 : (int8_t)0;
        int8_t by1 = (int8_t)(wrappingAbs(this->rng_->nextInt() % 3) << 6);
        byArray1[3] = (int8_t)(by1 | i2->level_);
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
        i2->addChest(byArray1);
        ++n3;
    }
}

SharedArray<int16_t> DungeonGen::rollRoom() {
    SharedArray<int16_t> sArray1(6);
    int32_t n1 = 3;
    int32_t n2 = 3;
    int32_t n3 = 31;
    int32_t n4 = 31;
    int32_t n5 = 4;
    int32_t n6 = 2 + wrappingAbs(this->rng_->nextInt()) % n5;
    int32_t n7 = 2 + wrappingAbs(this->rng_->nextInt()) % n5;
    int32_t n8 = n3 - n2 + 1 - (n6 - 1);
    int32_t n9 = n4 - n1 + 1 - (n7 - 1);
    sArray1[0] = (int16_t)(n2 + wrappingAbs(this->rng_->nextInt()) % n8);
    sArray1[1] = (int16_t)(n1 + wrappingAbs(this->rng_->nextInt()) % n9);
    sArray1[2] = (int16_t)(sArray1[0] + (n6 - 1));
    sArray1[3] = (int16_t)(sArray1[1] + (n7 - 1));
    sArray1[4] = (int16_t)(sArray1[0] + wrappingAbs(this->rng_->nextInt()) % n6);
    sArray1[5] = (int16_t)(sArray1[1] + wrappingAbs(this->rng_->nextInt()) % n7);
    return sArray1;
}

bool DungeonGen::tryPlaceRoom(SharedArray<int16_t> sArray, Dungeon *i2) {
    int32_t n1 = sArray[0] - 1 >= 0 ? sArray[0] - 1 : 0;
    int32_t n2 = sArray[2] + 1 <= 34 ? sArray[2] + 1 : 34;
    int32_t n3 = sArray[1] - 1 >= 0 ? sArray[1] - 1 : 0;
    int32_t n4 = sArray[3] + 1 <= 34 ? sArray[3] + 1 : 34;
    int32_t n5 = n1;
    while (n5 <= n2) {
        int32_t n6 = n3;
        while (n6 <= n4) {
            if (i2->tiles_[n5][n6] == 0) {
                return false;
            }
            ++n6;
        }
        ++n5;
    }
    this->carveRect(i2, sArray[0], (int16_t)(sArray[2] - sArray[0] + 1), sArray[1],
            (int16_t)(sArray[3] - sArray[1] + 1));
    if (sArray[2] != sArray[0] && sArray[3] != sArray[1]) {
        SharedArray<int16_t> sArray1(6);
        int32_t n7 = 0;
        while (n7 < 6) {
            sArray1[n7] = sArray[n7];
            ++n7;
        }
        this->rooms_.push_back(sArray1);
    }
    return true;
}

void DungeonGen::linkRooms(Dungeon *i2) {
    int32_t n1 = (int32_t)this->roomKeys_.size();
    int32_t n2 = 0;
    while (n2 < n1) {
        int32_t n3 = this->roomKeys_[(size_t)n2];
        int32_t n4 = (int32_t)this->unlinkedRooms_.size();
        int32_t n5 = 0x7FFFFFFF;
        int32_t n6 = 0;
        bool haveN6 = false;
        int32_t n7 = -1;
        int32_t n8 = 0;
        while (n8 < n4) {
            int32_t n9 = this->unlinkedRooms_[(size_t)n8];
            if (n9 != n3) {
                int32_t n10 = this->distanceSquared(n3, n9);
                if (n10 < n5) {
                    n5 = n10;
                    n6 = n9;
                    haveN6 = true;
                }
            } else {
                n7 = n8;
            }
            ++n8;
        }
        if (haveN6) {
            this->carveCorridor(i2, n3, n6);
        }
        if (n7 != -1) {
            this->unlinkedRooms_.erase(this->unlinkedRooms_.begin() + n7);
        }
        ++n2;
    }
}

int32_t DungeonGen::carveEntrance(Dungeon *i2, int16_t s) {
    int32_t n1 = -1;
    if (s == 1) {
        this->carveRect(i2, 17, 1, 0, 5);
        n1 = this->packKey((int16_t)17, (int16_t)4);
    } else if (s == 3) {
        this->carveRect(i2, 17, 1, 30, 5);
        n1 = this->packKey((int16_t)17, (int16_t)30);
    } else if (s == 4) {
        this->carveRect(i2, 0, 5, 17, 1);
        n1 = this->packKey((int16_t)4, (int16_t)17);
    } else if (s == 2) {
        this->carveRect(i2, 30, 5, 17, 1);
        n1 = this->packKey((int16_t)30, (int16_t)17);
    }
    return n1;
}

void DungeonGen::carveCorridor(Dungeon *i2, int32_t n, int32_t n2) {
    SharedArray<int16_t> sArray1 = this->unpackKey(n);
    int16_t s1 = sArray1[0];
    int16_t s2 = sArray1[1];
    sArray1 = this->unpackKey(n2);
    int16_t s3 = sArray1[0];
    int16_t s4 = sArray1[1];
    int32_t n1 = wrappingAbs(this->rng_->nextInt() % 2);
    if (n1 == 0) {
        if (s3 > s1) {
            this->carveRect(i2, s1, s3 - s1 + 1, s2, 1);
        } else {
            this->carveRect(i2, s3, s1 - s3 + 1, s2, 1);
        }
        if (s4 > s2) {
            this->carveRect(i2, s3, 1, s2, s4 - s2 + 1);
        } else {
            this->carveRect(i2, s3, 1, s4, s2 - s4 + 1);
        }
    } else {
        if (s4 > s2) {
            this->carveRect(i2, s1, 1, s2, s4 - s2 + 1);
        } else {
            this->carveRect(i2, s1, 1, s4, s2 - s4 + 1);
        }
        if (s3 > s1) {
            this->carveRect(i2, s1, s3 - s1 + 1, s4, 1);
        } else {
            this->carveRect(i2, s3, s1 - s3 + 1, s4, 1);
        }
    }
}

void DungeonGen::carveRect(Dungeon *i2, int32_t n, int32_t n2, int32_t n3, int32_t n4) {
    int32_t n1 = n;
    while (n1 < n + n2) {
        int32_t n5 = n3;
        while (n5 < n3 + n4) {
            if (i2->tiles_[n1][n5] != 8 && i2->tiles_[n1][n5] != 32) {
                i2->tiles_[n1][n5] = 0;
            }
            ++n5;
        }
        ++n1;
    }
}

int32_t DungeonGen::distanceSquared(int32_t n, int32_t n2) {
    SharedArray<int16_t> sArray1 = this->unpackKey(n);
    int16_t s1 = sArray1[0];
    int16_t s2 = sArray1[1];
    SharedArray<int16_t> sArray2 = this->unpackKey(n2);
    int16_t s3 = sArray2[0];
    int16_t s4 = sArray2[1];
    return (s3 - s1) * (s3 - s1) + (s4 - s2) * (s4 - s2);
}

int32_t DungeonGen::packKey(int16_t s, int16_t s2) {
    return (s << 16) | s2;
}

SharedArray<int16_t> DungeonGen::unpackKey(int32_t n) {
    SharedArray<int16_t> sArray1{(int16_t)(unsignedShiftRight32(0xFFFF0000 & n, 16)), (int16_t)(0xFFFF & n)};
    return sArray1;
}

void DungeonGen::addRoomKey(int32_t n) {
    this->roomKeys_.push_back(n);
    this->unlinkedRooms_.push_back(n);
}

bool DungeonGen::isExitDirection(int16_t s) {
    switch (s) {
        case 1:
        case 2:
        case 3:
        case 4: {
            return true;
        }
    }
    return false;
}

}
