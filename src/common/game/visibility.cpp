#include "src/common/game/visibility.hpp"

#include "src/common/game/util.hpp"

namespace visibility {

bool blocks(const Slot &slot) {
    return slot.is(SlotState::Wall) || slot.is(SlotState::Occluded);
}

void reset(const View &view) {
    int8_t by1 = 0;
    int32_t n1 = 0;
    while (n1 < kSlotCount) {
        (*view.slots)[(size_t)n1] = SlotState::Empty;
        ++n1;
    }
    by1 = view.peekAt(-1, 1);
    if (GameUtil::hasFlag((int8_t)1, by1)) {
        (*view.slots)[0] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(0, 1))) {
        (*view.slots)[1] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(1, 1))) {
        (*view.slots)[2] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(-2, 2))) {
        (*view.slots)[3] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(-1, 2))) {
        (*view.slots)[4] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(0, 2))) {
        (*view.slots)[5] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(1, 2))) {
        (*view.slots)[6] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(2, 2))) {
        (*view.slots)[7] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(-2, 3))) {
        (*view.slots)[8] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(-1, 3))) {
        (*view.slots)[9] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(0, 3))) {
        (*view.slots)[10] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(1, 3))) {
        (*view.slots)[11] = SlotState::Wall;
    }
    if (GameUtil::hasFlag((int8_t)1, by1 = view.peekAt(2, 3))) {
        (*view.slots)[12] = SlotState::Wall;
    }
    if (blocks((*view.slots)[0])) {
        (*view.slots)[4] = SlotState::Occluded;
        (*view.slots)[8] = SlotState::Occluded;
        (*view.slots)[9] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[1])) {
        int32_t n2 = 0;
        while (n2 < kSlotCount) {
            if (n2 != 1) {
                (*view.slots)[n2] = SlotState::Occluded;
            }
            ++n2;
        }
    }
    if (blocks((*view.slots)[2])) {
        (*view.slots)[6] = SlotState::Occluded;
        (*view.slots)[11] = SlotState::Occluded;
        (*view.slots)[12] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[3])) {
        (*view.slots)[8] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[4])) {
        (*view.slots)[8] = SlotState::Occluded;
        (*view.slots)[9] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[5])) {
        (*view.slots)[9] = SlotState::Occluded;
        (*view.slots)[10] = SlotState::Occluded;
        (*view.slots)[11] = SlotState::Occluded;
        (*view.slots)[4] = SlotState::Occluded;
        (*view.slots)[6] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[6])) {
        (*view.slots)[11] = SlotState::Occluded;
        (*view.slots)[12] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[7])) {
        (*view.slots)[12] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[9])) {
        (*view.slots)[8] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[10])) {
        (*view.slots)[9] = SlotState::Occluded;
        (*view.slots)[11] = SlotState::Occluded;
    }
    if (blocks((*view.slots)[11])) {
        (*view.slots)[12] = SlotState::Occluded;
    }
}

}
