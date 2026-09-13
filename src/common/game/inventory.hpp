#ifndef COMMON_GAME_INVENTORY_HPP
#define COMMON_GAME_INVENTORY_HPP

#include "src/common/runtime.hpp"

#include "src/common/game/equipment.hpp"

namespace inventory {

const int32_t kCapacity = 24;

const int32_t kSkillCount = 14;

struct Bag {
    SharedArray<int8_t> *items = nullptr;
    SharedArray<int32_t> *data = nullptr;
    SharedArray<int8_t> *equipped = nullptr;
    int8_t *count = nullptr;
};

bool hasRoom(int32_t count);

bool isEquipped(const SharedArray<int8_t> &items, int32_t index);

int32_t findEquipped(const SharedArray<int8_t> &items, int32_t count, int32_t id);

int32_t skillAt(const SharedArray<SharedArray<int16_t>> &skills, int32_t n);

bool addItem(Bag &bag, int32_t id, int32_t quantity, int32_t charge);

void unequipItem(Bag &bag, int32_t index);

void unequipSlot(Bag &bag, int32_t slot);

bool equip(Bag &bag, int32_t index, bool replace);

bool removeItem(Bag &bag, int32_t index);

}

#endif
