#ifndef COMMON_GAME_WORLD_STATE_HPP
#define COMMON_GAME_WORLD_STATE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "src/common/runtime.hpp"

namespace worldstate {

using DroppedItemRecord = SharedArray<int8_t>;
using DroppedItemList = std::vector<DroppedItemRecord>;
using ChestRecord = SharedArray<int8_t>;
using ChestList = std::vector<ChestRecord>;
using MonsterRecord = SharedArray<int8_t>;
using MonsterList = std::vector<MonsterRecord>;

class DroppedItems {
public:
    static constexpr std::size_t kDungeonCount = 37;

    DroppedItemList &at(std::size_t dungeonIndex) { return byDungeon_.at(dungeonIndex); }
    const DroppedItemList &at(std::size_t dungeonIndex) const {
        return byDungeon_.at(dungeonIndex);
    }

    void add(std::size_t dungeonIndex, const DroppedItemRecord &record) {
        at(dungeonIndex).push_back(record);
    }

    bool removeSame(std::size_t dungeonIndex, const DroppedItemRecord &record) {
        DroppedItemList &items = at(dungeonIndex);
        const auto match = std::find_if(items.begin(), items.end(), [&](const auto &candidate) {
            return candidate.sameRef(record);
        });
        if (match == items.end()) return false;
        items.erase(match);
        return true;
    }

private:
    std::array<DroppedItemList, kDungeonCount> byDungeon_;
};

class Chests {
public:
    static constexpr std::size_t kDungeonCount = 37;

    void initializeTables(std::size_t firstDungeonIndex) {
        present_.fill(false);
        for (std::size_t dungeon = 0; dungeon < kDungeonCount; ++dungeon) {
            byDungeon_[dungeon].clear();
        }
        for (std::size_t dungeon = firstDungeonIndex; dungeon < kDungeonCount; ++dungeon) {
            present_[dungeon] = true;
        }
    }

    bool hasTable(std::size_t dungeonIndex) const { return present_.at(dungeonIndex); }

    ChestList &at(std::size_t dungeonIndex) {
        if (!hasTable(dungeonIndex)) throw std::logic_error("chest table is absent");
        return byDungeon_.at(dungeonIndex);
    }
    const ChestList &at(std::size_t dungeonIndex) const {
        if (!hasTable(dungeonIndex)) throw std::logic_error("chest table is absent");
        return byDungeon_.at(dungeonIndex);
    }

    void put(std::size_t dungeonIndex, const ChestRecord &record) {
        ChestList &items = at(dungeonIndex);
        const auto match = std::find_if(items.begin(), items.end(), [&](const auto &candidate) {
            return candidate[0] == record[0] && candidate[1] == record[1];
        });
        if (match == items.end()) {
            items.push_back(record);
        } else {
            *match = record;
        }
    }

    ChestRecord findAt(std::size_t dungeonIndex, int32_t x, int32_t y) const {
        if (!hasTable(dungeonIndex)) return ChestRecord();
        const ChestList &items = at(dungeonIndex);
        const auto match = std::find_if(items.begin(), items.end(), [&](const auto &candidate) {
            return candidate[0] == x && candidate[1] == y;
        });
        return match == items.end() ? ChestRecord() : *match;
    }

    bool removeAt(std::size_t dungeonIndex, int32_t x, int32_t y) {
        if (!hasTable(dungeonIndex)) return false;
        ChestList &items = at(dungeonIndex);
        const auto match = std::find_if(items.begin(), items.end(), [&](const auto &candidate) {
            return candidate[0] == x && candidate[1] == y;
        });
        if (match == items.end()) return false;
        items.erase(match);
        return true;
    }

private:
    std::array<ChestList, kDungeonCount> byDungeon_;
    std::array<bool, kDungeonCount> present_{};
};

enum class MonsterKey {
    Coordinates,
    Uid,
};

class Monsters {
public:
    static constexpr std::size_t kDungeonCount = 37;

    void initializeTables(std::size_t firstDungeonIndex, MonsterKey key) {
        key_ = key;
        present_.fill(false);
        for (std::size_t dungeon = 0; dungeon < kDungeonCount; ++dungeon) {
            byDungeon_[dungeon].clear();
        }
        for (std::size_t dungeon = firstDungeonIndex; dungeon < kDungeonCount; ++dungeon) {
            present_[dungeon] = true;
        }
    }

    bool hasTable(std::size_t dungeonIndex) const { return present_.at(dungeonIndex); }
    MonsterKey key() const { return key_; }

    MonsterList &at(std::size_t dungeonIndex) {
        if (!hasTable(dungeonIndex)) throw std::logic_error("monster table is absent");
        return byDungeon_.at(dungeonIndex);
    }
    const MonsterList &at(std::size_t dungeonIndex) const {
        if (!hasTable(dungeonIndex)) throw std::logic_error("monster table is absent");
        return byDungeon_.at(dungeonIndex);
    }

    void put(std::size_t dungeonIndex, const MonsterRecord &record) {
        MonsterList &items = at(dungeonIndex);
        const auto match = std::find_if(items.begin(), items.end(), [&](const auto &candidate) {
            return sameKey(candidate, record);
        });
        if (match == items.end()) {
            items.push_back(record);
        } else {
            *match = record;
        }
    }

    MonsterRecord findAt(std::size_t dungeonIndex, int32_t x, int32_t y) const {
        if (!hasTable(dungeonIndex)) return MonsterRecord();
        return find(dungeonIndex, [&](const auto &record) {
            return record[4] == x && record[5] == y;
        });
    }

    MonsterRecord findUid(std::size_t dungeonIndex, int32_t uid) const {
        if (!hasTable(dungeonIndex)) return MonsterRecord();
        return find(dungeonIndex, [&](const auto &record) { return decodeUid(record) == uid; });
    }

    MonsterRecord removeAt(std::size_t dungeonIndex, int32_t x, int32_t y) {
        return remove(dungeonIndex, [&](const auto &record) {
            return record[4] == x && record[5] == y;
        });
    }

    MonsterRecord removeUid(std::size_t dungeonIndex, int32_t uid) {
        return remove(dungeonIndex,
                      [&](const auto &record) { return decodeUid(record) == uid; });
    }

private:
    static int32_t decodeUid(const MonsterRecord &record) {
        const int16_t high = (int16_t)(record[0] & 0xFF);
        const int16_t low = (int16_t)(record[1] & 0xFF);
        return (int16_t)((high << 8) | low);
    }

    bool sameKey(const MonsterRecord &left, const MonsterRecord &right) const {
        if (key_ == MonsterKey::Coordinates) {
            return left[4] == right[4] && left[5] == right[5];
        }
        return decodeUid(left) == decodeUid(right);
    }

    template <typename Predicate>
    MonsterRecord find(std::size_t dungeonIndex, Predicate predicate) const {
        const MonsterList &items = at(dungeonIndex);
        const auto match = std::find_if(items.begin(), items.end(), predicate);
        return match == items.end() ? MonsterRecord() : *match;
    }

    template <typename Predicate>
    MonsterRecord remove(std::size_t dungeonIndex, Predicate predicate) {
        if (!hasTable(dungeonIndex)) return MonsterRecord();
        MonsterList &items = at(dungeonIndex);
        const auto match = std::find_if(items.begin(), items.end(), predicate);
        if (match == items.end()) return MonsterRecord();
        MonsterRecord removed = *match;
        items.erase(match);
        return removed;
    }

    MonsterKey key_ = MonsterKey::Coordinates;
    std::array<MonsterList, kDungeonCount> byDungeon_;
    std::array<bool, kDungeonCount> present_{};
};

struct NpcState {
    static constexpr std::size_t kNpcCount = 9;
    static constexpr std::size_t kFactionCount = 4;

    void initialize() {
        firstMeeting.fill(true);
        npcPresent.fill(true);
    }

    void resetPerLevel() {
        befriendDone.fill(0);
        threatenDone.fill(0);
    }

    std::array<bool, kNpcCount> firstMeeting{};
    std::array<bool, 7> npcPresent{};
    std::array<int8_t, kFactionCount> befriendDone{};
    std::array<int8_t, kFactionCount> threatenDone{};
    std::array<int16_t, kFactionCount> interactionCount{};
    std::array<int16_t, kFactionCount> aidPoints{};
    std::array<int16_t, kFactionCount> suspicion{};
    int16_t scrapCount = 0;
    int16_t gemCount = 0;
    int8_t wardenVisits = 0;
    bool wardenPresent = false;
    bool wardenPending = false;
};

struct WorldState {
    platform::PlatformContext *platformContext = nullptr;
    GameRandom *random = nullptr;
    int16_t nextMonsterUid() {
        monsterUid = (int16_t)(monsterUid + 1);
        return monsterUid;
    }

    DroppedItems droppedItems;
    Chests chests;
    Monsters monsters;
    NpcState npcs;
    int16_t monsterUid = 0;
};

}

#endif
