#include "src/common/game/inventory.hpp"

#include "src/common/game/items.hpp"

namespace inventory {

bool hasRoom(int32_t count) { return count < kCapacity; }

bool isEquipped(const SharedArray<int8_t> &items, int32_t index) {
    int8_t id = items[index];
    if (!Items::isUsable(wrappingAbs(id))) return false;
    return id < 0;
}

int32_t findEquipped(const SharedArray<int8_t> &items, int32_t count, int32_t id) {
    int32_t wanted = -wrappingAbs(id);
    for (int32_t n = 0; n < count; ++n) {
        if (items[n] == wanted) return n;
    }
    return -1;
}

int32_t skillAt(const SharedArray<SharedArray<int16_t>> &skills, int32_t n) {
    int32_t seen = 0;
    for (int32_t skill = 0; skill < kSkillCount; ++skill) {
        if (skills[skill][0] > 0) {
            if (seen == n) return skill;
            ++seen;
        }
    }
    return -1;
}

bool addItem(Bag &bag, int32_t id, int32_t quantity, int32_t charge) {
    if (!hasRoom(*bag.count)) return false;

    (*bag.items)[*bag.count] = (int8_t)id;
    (*bag.data)[*bag.count] = (quantity << 16) + (int8_t)charge;
    *bag.count = (int8_t)(*bag.count + 1);
    return true;
}

void unequipItem(Bag &bag, int32_t index) {
    if (!isEquipped(*bag.items, index)) return;
    if (index < 0 || index >= kCapacity) return;

    int8_t id = (int8_t)wrappingAbs((*bag.items)[index]);
    (*bag.items)[index] = id;

    for (int32_t slot = 0; slot < equipment::kStoredSlotCount; ++slot) {
        if ((*bag.equipped)[slot] == id) {
            (*bag.equipped)[slot] = 0;
            break;
        }
    }
}

void unequipSlot(Bag &bag, int32_t slot) {
    for (int32_t n = 0; n < *bag.count; ++n) {
        int8_t id = (int8_t)wrappingAbs((*bag.items)[n]);
        if (Items::effectOf(id) == slot) unequipItem(bag, n);
    }
}

bool equip(Bag &bag, int32_t index, bool replace) {
    int8_t id = (*bag.items)[index];
    if (id < 0) return false;
    if (!Items::isUsable(id)) return false;

    int32_t slot = Items::effectOf(id);
    if ((*bag.equipped)[slot] != 0) {
        if (!replace) return false;
        unequipSlot(bag, slot);
    }

    (*bag.equipped)[slot] = id;
    (*bag.items)[index] = (int8_t)(-wrappingAbs((*bag.items)[index]));
    return true;
}

bool removeItem(Bag &bag, int32_t index) {
    if (index < 0 || index >= *bag.count) return false;

    unequipItem(bag, index);
    (*bag.items)[index] = 0;

    for (int32_t n = index; n < *bag.count - 1; ++n) {
        (*bag.items)[n] = (*bag.items)[n + 1];
        (*bag.data)[n] = (*bag.data)[n + 1];
    }
    *bag.count = (int8_t)(*bag.count - 1);
    return true;
}

}
