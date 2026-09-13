#include "src/stormhold/dungeon_gen.hpp"

#include "src/common/game/util.hpp"
#include "src/stormhold/dungeon.hpp"

namespace stormhold {

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
    int32_t n4 = 0;
    while (n4 < 3) {
        int32_t n5 = 0;
        while (n5 < 3) {
            campGrid_[8 + n4][8 + n5] = 0;
            ++n5;
        }
        ++n4;
    }
    campGrid_[4][8] = 0;
    campGrid_[4][7] = 0;
    campGrid_[3][7] = 0;
    campGrid_[16][8] = 0;
    campGrid_[16][7] = 0;
    campGrid_[15][7] = 0;
    campGrid_[8][3] = 0;
    campGrid_[7][3] = 0;
    campGrid_[7][2] = 0;
    campGrid_[10][4] = 0;
    campGrid_[11][4] = 0;
    campGrid_[12][4] = 0;
    campGrid_[12][3] = 0;
    campGrid_[6][14] = 0;
    campGrid_[7][14] = 0;
    campGrid_[8][14] = 0;
    campGrid_[9][14] = 0;
    campGrid_[10][14] = 0;
    campGrid_[11][14] = 0;
    campGrid_[12][14] = 0;
    campGrid_[6][13] = 0;
    campGrid_[12][13] = 0;
}

void DungeonGen::generate(Dungeon *i2) {
    int64_t l1 = i2->id_ * 5000;
    i2->tiles_ = makeSharedArray2D<int8_t>(i2->width_, i2->height_);
    int16_t s1 = i2->geometry_[4];
    int16_t s2 = i2->geometry_[5];
    i2->rooms_.clear();
    this->generateLayout(i2, l1, s1, s2);
}

void DungeonGen::generateLayout(Dungeon *i2, int64_t l, int16_t s, int16_t s2) {
    int32_t n1;
    i2->exitDir1_ = s;
    i2->exitDir2_ = s2;
    int32_t n2 = -1;
    i2->rng_ = new GameRandom(l);
    int32_t n3 = 0;
    while (n3 < 35) {
        n1 = 0;
        while (n1 < 35) {
            i2->tiles_[n3][n1] = 1;
            ++n1;
        }
        ++n3;
    }
    i2->roomKeys_.clear();
    i2->unlinkedRooms_.clear();
    i2->genMinX_ = (int16_t)3;
    i2->genMinY_ = (int16_t)3;
    i2->genMaxX_ = (int16_t)31;
    i2->genMaxY_ = (int16_t)31;
    if (this->isExitDirection(s) && (n2 = this->carveEntrance(i2, s)) >= 0) {
        this->addRoomKey(i2, n2);
    }
    if (this->isExitDirection(s2) && (n2 = this->carveEntrance(i2, s2)) >= 0) {
        this->addRoomKey(i2, n2);
    }
    n1 = 0;
    int32_t n4 = 0;
    while (n4 < 15) {
        i2->roomScratch_ = this->rollRoom(i2);
        if (!this->tryPlaceRoom(i2, i2->roomScratch_)) continue;
        int32_t n5 = this->packKey(i2->roomScratch_[4], i2->roomScratch_[5]);
        this->addRoomKey(i2, n5);
        if (++n4 < 2 || n1 != 0) continue;
        int16_t s1 = i2->roomScratch_[4];
        int16_t s3 = i2->roomScratch_[5];
        int16_t s4 = (int16_t)(i2->roomScratch_[2] - i2->roomScratch_[0] + 1);
        int16_t s5 = (int16_t)(i2->roomScratch_[3] - i2->roomScratch_[1] + 1);
        if (s4 < 3 || s5 < 3) continue;
        int16_t s6 = (int16_t)(i2->roomScratch_[0] + s4 / 2);
        int16_t s7 = (int16_t)(i2->roomScratch_[1] + s5 / 2);
        if (s6 == s1 && s7 == s3) continue;
        SharedArray<int8_t> byArray1 = i2->tiles_[s6];
        int16_t s8 = s7;
        byArray1[s8] = (int8_t)(byArray1[s8] | 8);
        n1 = 1;
    }
    this->linkRooms(i2);
}

SharedArray<int16_t> DungeonGen::rollRoom(Dungeon *i2) {
    int32_t n1 = 4;
    int32_t n2 = 2 + wrappingAbs(i2->rng_->nextInt()) % n1;
    int32_t n3 = 2 + wrappingAbs(i2->rng_->nextInt()) % n1;
    int32_t n4 = i2->genMaxX_ - i2->genMinY_ + 1 - (n2 - 1);
    int32_t n5 = i2->genMaxY_ - i2->genMinX_ + 1 - (n3 - 1);
    i2->roomScratch_[0] = (int16_t)(i2->genMinY_ + wrappingAbs(i2->rng_->nextInt()) % n4);
    i2->roomScratch_[1] = (int16_t)(i2->genMinX_ + wrappingAbs(i2->rng_->nextInt()) % n5);
    i2->roomScratch_[2] = (int16_t)(i2->roomScratch_[0] + (n2 - 1));
    i2->roomScratch_[3] = (int16_t)(i2->roomScratch_[1] + (n3 - 1));
    i2->roomScratch_[4] = (int16_t)(i2->roomScratch_[0] + wrappingAbs(i2->rng_->nextInt()) % n2);
    i2->roomScratch_[5] = (int16_t)(i2->roomScratch_[1] + wrappingAbs(i2->rng_->nextInt()) % n3);
    return i2->roomScratch_;
}

bool DungeonGen::tryPlaceRoom(Dungeon *i2, SharedArray<int16_t> sArray) {
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
    this->carveRect(i2, (int32_t)sArray[0], (int32_t)((int16_t)(sArray[2] - sArray[0] + 1)), (int32_t)sArray[1],
            (int16_t)(sArray[3] - sArray[1] + 1));
    if (sArray[2] != sArray[0] && sArray[3] != sArray[1]) {
        SharedArray<int16_t> sArray1(6);
        int32_t n7 = 0;
        while (n7 < 6) {
            sArray1[n7] = sArray[n7];
            ++n7;
        }
        i2->rooms_.push_back(sArray1);
    }
    return true;
}

void DungeonGen::linkRooms(Dungeon *i2) {
    int32_t n1 = (int32_t)i2->roomKeys_.size();
    int32_t n2 = 0;
    while (n2 < n1) {
        int32_t n3 = i2->roomKeys_[(size_t)n2];
        int32_t n4 = (int32_t)i2->unlinkedRooms_.size();
        int32_t n5 = 0x7FFFFFFF;
        bool haveN6 = false;
        int32_t n6 = 0;
        int32_t n7 = -1;
        int32_t n8 = 0;
        while (n8 < n4) {
            int32_t n9 = i2->unlinkedRooms_[(size_t)n8];
            if (n9 != n3) {
                int32_t n10 = this->distanceSquared(i2, n3, n9);
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
            i2->unlinkedRooms_.erase(i2->unlinkedRooms_.begin() + n7);
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
    SharedArray<int16_t> sArray1 = this->unpackKey(i2, n);
    int16_t s1 = sArray1[0];
    int16_t s2 = sArray1[1];
    sArray1 = this->unpackKey(i2, n2);
    int16_t s3 = sArray1[0];
    int16_t s4 = sArray1[1];
    int32_t n1 = wrappingAbs(i2->rng_->nextInt() % 2);
    if (n1 == 0) {
        if (s3 > s1) {
            this->carveRect(i2, (int32_t)s1, s3 - s1 + 1, (int32_t)s2, 1);
        } else {
            this->carveRect(i2, (int32_t)s3, s1 - s3 + 1, (int32_t)s2, 1);
        }
        if (s4 > s2) {
            this->carveRect(i2, (int32_t)s3, 1, (int32_t)s2, s4 - s2 + 1);
        } else {
            this->carveRect(i2, (int32_t)s3, 1, (int32_t)s4, s2 - s4 + 1);
        }
    } else {
        if (s4 > s2) {
            this->carveRect(i2, (int32_t)s1, 1, (int32_t)s2, s4 - s2 + 1);
        } else {
            this->carveRect(i2, (int32_t)s1, 1, (int32_t)s4, s2 - s4 + 1);
        }
        if (s3 > s1) {
            this->carveRect(i2, (int32_t)s1, s3 - s1 + 1, (int32_t)s4, 1);
        } else {
            this->carveRect(i2, (int32_t)s3, s1 - s3 + 1, (int32_t)s4, 1);
        }
    }
}

void DungeonGen::carveRect(Dungeon *i2, int32_t n, int32_t n2, int32_t n3, int32_t n4) {
    int32_t n1 = n;
    while (n1 < n + n2) {
        int32_t n5 = n3;
        while (n5 < n3 + n4) {
            if (i2->tiles_[n1][n5] != 8) {
                i2->tiles_[n1][n5] = 0;
            }
            ++n5;
        }
        ++n1;
    }
}

int32_t DungeonGen::packKey(int16_t s, int16_t s2) {
    return s << 16 | s2;
}

SharedArray<int16_t> DungeonGen::unpackKey(Dungeon *i2, int32_t n) {
    i2->keyScratch_[0] = (int16_t)(unsignedShiftRight32(0xFFFF0000 & n, 16));
    i2->keyScratch_[1] = (int16_t)(0xFFFF & n);
    return i2->keyScratch_;
}

void DungeonGen::addRoomKey(Dungeon *i2, int32_t n) {
    i2->roomKeys_.push_back(n);
    i2->unlinkedRooms_.push_back(n);
}

int32_t DungeonGen::distanceSquared(Dungeon *i2, int32_t n, int32_t n2) {
    SharedArray<int16_t> sArray1 = this->unpackKey(i2, n);
    int16_t s1 = sArray1[0];
    int16_t s2 = sArray1[1];
    SharedArray<int16_t> sArray2 = this->unpackKey(i2, n2);
    int16_t s3 = sArray2[0];
    int16_t s4 = sArray2[1];
    return (s3 - s1) * (s3 - s1) + (s4 - s2) * (s4 - s2);
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
