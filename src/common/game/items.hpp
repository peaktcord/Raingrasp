#ifndef COMMON_GAME_ITEMS_HPP
#define COMMON_GAME_ITEMS_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

class Items {
public:
    struct Entry {
        std::string name;

        int8_t category = 0;

        int8_t tier = 0;

        int8_t rating = 0;

        int16_t buyPrice = 0;
        int16_t sellPrice = 0;

        int8_t effect = 0;
    };

    enum Stat {
        CATEGORY = 1,
        TIER = 2,
        RATING = 3,
        BUY_PRICE = 4,
        SELL_PRICE = 5,
        EFFECT = 6
    };

    static int32_t categoryCount_;
    static SharedArray<std::string> categoryNames_;

    static int32_t count_;
    static SharedArray<Entry> items_;

    static int8_t droppedRows_;
    static SharedArray<SharedArray<int8_t>> droppedTable_;

    static int16_t nextId_;

    static int16_t nextId();
    static int32_t indexOf(int32_t id) { return id - 1; }
    static Entry &byId(int32_t id) { return items_[id - 1]; }
    static Entry &at(int32_t index) { return items_[index]; }

    static bool isUsable(int32_t id);
    static std::string nameOf(int32_t id);
    static int32_t effectOf(int32_t id);
    static int32_t stat(int32_t which, int32_t id);

    static bool isWeaponOrArmour(int32_t id);
    static SharedArray<std::string> weaponCategoryNames();

    static void loadAll(platform::PlatformContext *context);
    static void loadItems(platform::PlatformContext *context);
    static void loadDropped(platform::PlatformContext *context);

    static int32_t randomOfTier(GameRandom *random, int32_t tier);
    static int32_t randomDropped(GameRandom *random, int32_t depth, int32_t draws);
};

#endif
