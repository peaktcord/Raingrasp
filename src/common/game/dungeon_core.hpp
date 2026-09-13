#ifndef COMMON_GAME_DUNGEON_CORE_HPP
#define COMMON_GAME_DUNGEON_CORE_HPP

#include <array>
#include <memory>
#include <stdexcept>
#include <utility>

#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

class Monster;

class DungeonCore {
public:
    static const int32_t kSpawnTable[37][4];
    static const int8_t kDungeonLevel[36];

    int8_t id_ = 0;
    int8_t level_ = 0;
    int16_t width_ = 0;
    int16_t height_ = 0;
    SharedArray<SharedArray<int8_t>> tiles_;
    int16_t exitDir1_ = 0;
    int16_t exitDir2_ = 0;
    bool populated_ = false;
    SharedArray<int8_t> geometry_;
    SharedArray<int32_t> slotScratch_ = SharedArray<int32_t>(2);
    bool visited_ = false;
    worldstate::WorldState &worldState_;

    DungeonCore(worldstate::WorldState &worldState, bool populated);
    virtual ~DungeonCore() = default;

    void assignLevel();

    bool isFree(int32_t x, int32_t y);

    void addChest(const SharedArray<int8_t> &record);
    void addDroppedItem(const SharedArray<int8_t> &record);
    void removeChest(const SharedArray<int8_t> &record);
    worldstate::DroppedItemList droppedItemsAt(int32_t x, int32_t y);

    virtual void spawnMonsters(int32_t count) = 0;

    virtual void scanSurroundings(int32_t x, int32_t y, int32_t facing,
                                  SharedArray<SharedArray<int8_t>> into) = 0;

    virtual Monster *monsterAt(Monster *into, int32_t x, int32_t y) = 0;
};

namespace worldstate {

class DungeonRegistry {
public:
    static constexpr std::size_t kDungeonCount = 37;

    constexpr std::size_t size() const { return kDungeonCount; }
    DungeonCore *operator[](std::size_t dungeonIndex) const {
        return byDungeon_.at(dungeonIndex).get();
    }
    DungeonCore *atId(int32_t dungeonId) const {
        if (dungeonId < 1 || dungeonId > (int32_t)kDungeonCount) {
            throw std::out_of_range("dungeon id is out of range");
        }
        return (*this)[(std::size_t)(dungeonId - 1)];
    }
    template <typename T>
    T *as(std::size_t dungeonIndex) const {
        return static_cast<T *>((*this)[dungeonIndex]);
    }

    template <typename T, typename... Args>
    T *emplace(std::size_t dungeonIndex, Args &&...args) {
        auto dungeon = std::make_unique<T>(std::forward<Args>(args)...);
        T *result = dungeon.get();
        byDungeon_.at(dungeonIndex) = std::move(dungeon);
        return result;
    }

    void clear() {
        for (auto &dungeon : byDungeon_) dungeon.reset();
    }

private:
    std::array<std::unique_ptr<DungeonCore>, kDungeonCount> byDungeon_;
};

}

#endif
