#include "src/common/game/dungeon_core.hpp"

#include "src/common/game/util.hpp"

const int32_t DungeonCore::kSpawnTable[37][4] = {
    {1, 2, 1, 3},     {6, 7, 8, 6},     {1, 2, 3, 1},     {6, 7, 8, 7},     {3, 4, 11, 12},
    {8, 9, 11, 12},   {3, 4, 12, 13},   {8, 9, 12, 13},   {4, 5, 12, 13},   {9, 10, 12, 13},
    {4, 5, 13, 14},   {9, 10, 13, 14},  {12, 13, 4, 5},   {12, 13, 9, 10},  {13, 14, 15, 16},
    {14, 15, 16, 17}, {15, 16, 17, 18}, {16, 17, 18, 21}, {17, 18, 19, 26}, {18, 19, 20, 21},
    {19, 20, 26, 27}, {21, 22, 26, 27}, {21, 22, 27, 28}, {22, 23, 27, 28}, {22, 23, 28, 29},
    {23, 24, 28, 29}, {23, 24, 29, 30}, {24, 25, 29, 30}, {26, 27, 28, 31}, {27, 28, 31, 32},
    {28, 29, 32, 33}, {29, 30, 33, 34}, {31, 32, 34, 35}, {32, 33, 36, 37}, {34, 35, 37, 38},
    {38, 39, 40, 35}, {38, 39, 40, 35}};

const int8_t DungeonCore::kDungeonLevel[36] = {1,  5,  9,  13, 14, 15, 22, 23, 24, 2,  6,  10,
                                               19, 20, 21, 31, 32, 33, 3,  7,  11, 16, 17, 18,
                                               28, 29, 30, 4,  8,  12, 25, 26, 27, 34, 35, 36};

DungeonCore::DungeonCore(worldstate::WorldState &worldState, bool populated)
    : populated_(populated), worldState_(worldState) {}

void DungeonCore::assignLevel() {
    this->level_ = 1;
    if (this->id_ >= 2 && this->id_ <= 37) {
        this->level_ = kDungeonLevel[this->id_ - 2];
    }
}

bool DungeonCore::isFree(int32_t n, int32_t n2) {
    if (n < 0 || n2 < 0 || n >= this->width_ || n2 >= this->height_) {
        return false;
    }
    int8_t by1 = this->tiles_[n][n2];
    if (GameUtil::hasFlag((int8_t)1, by1)) {
        return false;
    }
    if (GameUtil::hasFlag((int8_t)2, by1)) {
        return false;
    }
    if (GameUtil::hasFlag((int8_t)8, by1)) {
        return false;
    }
    return !GameUtil::hasFlag((int8_t)32, by1);
}

void DungeonCore::addChest(const SharedArray<int8_t> &record) {
    worldState_.chests.put((std::size_t)(this->id_ - 1), record);
    this->tiles_[record[0]][record[1]] = (int8_t)(this->tiles_[record[0]][record[1]] | 0x10);
}

void DungeonCore::addDroppedItem(const SharedArray<int8_t> &byArray) {
    int8_t by1 = byArray[0];
    int8_t by2 = byArray[1];
    worldState_.droppedItems.add((std::size_t)(this->id_ - 1), byArray);
    SharedArray<int8_t> byArray1 = this->tiles_[by1];
    int8_t by3 = by2;
    byArray1[by3] = (int8_t)(byArray1[by3] | 4);
}

void DungeonCore::removeChest(const SharedArray<int8_t> &byArray) {
    int8_t by1 = byArray[0];
    int8_t by2 = byArray[1];
    int8_t by3 = this->tiles_[by1][by2];
    if (GameUtil::hasFlag((int8_t)1, by3)) {
        return;
    }
    if (!GameUtil::hasFlag((int8_t)16, by3)) {
        return;
    }
    worldState_.chests.removeAt((std::size_t)(this->id_ - 1), by1, by2);
    this->tiles_[by1][by2] = GameUtil::clearFlag((int8_t)16, this->tiles_[by1][by2]);
}

worldstate::DroppedItemList DungeonCore::droppedItemsAt(int32_t n, int32_t n2) {
    worldstate::DroppedItemList items;
    for (const SharedArray<int8_t> &byArray1 : worldState_.droppedItems.at((std::size_t)(this->id_ - 1))) {
        if (byArray1[0] != n || byArray1[1] != n2) continue;
        items.push_back(byArray1);
    }
    return items;
}
