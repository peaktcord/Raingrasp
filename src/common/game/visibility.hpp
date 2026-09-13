#ifndef COMMON_GAME_VISIBILITY_HPP
#define COMMON_GAME_VISIBILITY_HPP

#include <array>
#include <variant>

#include "src/common/runtime.hpp"

namespace visibility {

constexpr int32_t kSlotCount = 13;

constexpr int32_t kEmpty = 0;
constexpr int32_t kWall = 1;
constexpr int32_t kOccluded = -1;

enum class SlotState : int32_t {
    Occluded = kOccluded,
    Empty = kEmpty,
    Wall = kWall,
};

class Slot {
public:
    Slot() : value_(SlotState::Empty) {}
    Slot(SlotState state) : value_(state) {}
    Slot(const SharedArray<int8_t> &record) : value_(record) {}
    Slot(const std::string &npc) : value_(npc) {}

    bool is(SlotState state) const {
        const SlotState *stored = std::get_if<SlotState>(&value_);
        return stored != nullptr && *stored == state;
    }
    const SharedArray<int8_t> *record() const { return std::get_if<SharedArray<int8_t>>(&value_); }
    const std::string *npc() const { return std::get_if<std::string>(&value_); }

    bool operator==(const Slot &other) const {
        if (value_.index() != other.value_.index()) return false;
        if (const SlotState *state = std::get_if<SlotState>(&value_)) {
            return *state == std::get<SlotState>(other.value_);
        }
        if (const SharedArray<int8_t> *bytes = std::get_if<SharedArray<int8_t>>(&value_)) {
            return bytes->sameRef(std::get<SharedArray<int8_t>>(other.value_));
        }
        return std::get<std::string>(value_) == std::get<std::string>(other.value_);
    }

private:
    std::variant<SlotState, SharedArray<int8_t>, std::string> value_;
};

using Slots = std::array<Slot, kSlotCount>;

constexpr int8_t kWallFlag = 1;

bool blocks(const Slot &slot);

struct View {
    Slots *slots = nullptr;

    int8_t (*peek)(void *self, int32_t column, int32_t depth) = nullptr;

    void *self = nullptr;

    int8_t peekAt(int32_t column, int32_t depth) const { return peek(self, column, depth); }
};

void reset(const View &view);

}

#endif
