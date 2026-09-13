#include "src/common/game/items.hpp"

#include "src/common/game/util.hpp"
#include "src/common/game/datfiles.hpp"

int32_t Items::categoryCount_ = 0;
SharedArray<std::string> Items::categoryNames_;
int32_t Items::count_ = 0;
SharedArray<Items::Entry> Items::items_;
int8_t Items::droppedRows_ = 0;
SharedArray<SharedArray<int8_t>> Items::droppedTable_;
int16_t Items::nextId_ = 0;

int16_t Items::nextId() {
    nextId_ = (int16_t)(nextId_ + 1);
    return nextId_;
}

bool Items::isUsable(int32_t id) { return byId(id).effect != -1; }

std::string Items::nameOf(int32_t id) { return byId(id).name; }

int32_t Items::effectOf(int32_t id) { return byId(id).effect; }

int32_t Items::stat(int32_t which, int32_t id) {
    const Entry &item = byId(id);
    switch (which) {
        case CATEGORY:
            return item.category;
        case TIER:
            return item.tier;
        case RATING:
            return item.rating;
        case BUY_PRICE:
            return item.buyPrice;
        case SELL_PRICE:
            return item.sellPrice;
        case EFFECT:
            return item.effect;
        default:
            return -1;
    }
}

bool Items::isWeaponOrArmour(int32_t id) {
    int8_t category = byId(id).category;
    return category >= 1 && category <= 10;
}

SharedArray<std::string> Items::weaponCategoryNames() {
    SharedArray<std::string> out(13);
    for (int32_t n = 0; n < 13; ++n) {
        out[n] = at(86 + n).name;
    }
    return out;
}

void Items::loadAll(platform::PlatformContext *context) {
    nextId_ = 0;
    loadItems(context);
    loadDropped(context);
}

void Items::loadItems(platform::PlatformContext *context) {
    BinaryReader *in = GameUtil::openDatFile(context, std::string("itemsin.dat"));
    categoryCount_ = in->readShort();
    categoryNames_ = SharedArray<std::string>(categoryCount_);
    for (int32_t n = 0; n < categoryCount_; ++n) categoryNames_[n] = in->readUTF();

    count_ = in->readShort();
    items_ = SharedArray<Entry>(count_);

    for (int32_t n = 0; n < count_; ++n) at(n).name = in->readUTF();
    for (int32_t n = 0; n < count_; ++n) at(n).category = in->readByte();
    for (int32_t n = 0; n < count_; ++n) at(n).tier = in->readByte();
    for (int32_t n = 0; n < count_; ++n) at(n).rating = in->readByte();
    for (int32_t n = 0; n < count_; ++n) at(n).buyPrice = in->readShort();
    for (int32_t n = 0; n < count_; ++n) at(n).sellPrice = in->readShort();
    for (int32_t n = 0; n < count_; ++n) at(n).effect = in->readByte();
    delete in;
}

void Items::loadDropped(platform::PlatformContext *context) {
    BinaryReader *in = GameUtil::openDatFile(context, std::string("droppeditemsin.dat"));
    datfiles::loadDroppedItems(in, &droppedRows_, &droppedTable_);
    delete in;
}

int32_t Items::randomOfTier(GameRandom *random, int32_t tier) {
    int32_t first = -1;
    int32_t last = -1;
    for (int32_t n = 0; n < count_; ++n) {
        if (at(n).category == 11 && at(n).tier == (int8_t)tier) {
            if (first == -1) {
                first = n;
            }
            last = n;
        }
    }
    int32_t span = last - first + 1;
    return 1 + (first + wrappingAbs(random->nextInt() % span));
}

int32_t Items::randomDropped(GameRandom *random, int32_t depth, int32_t draws) {
    int32_t best = GameUtil::randomInt(random, 100);
    for (int32_t n = 1; n < draws; ++n) {
        int32_t roll = GameUtil::randomInt(random, 100);
        if (roll > best) {
            best = roll;
        }
    }
    int32_t column = best <= 64 ? 0 : (best <= 75 ? 1 : (best <= 90 ? 3 : 4));
    int32_t row = GameUtil::randomInt(random, 10) + depth - 2;
    if (row > droppedRows_ - 1) {
        row = droppedRows_ - 1;
    }
    if (row < 0) {
        row = 0;
    }
    int32_t id = droppedTable_[row][column];
    if (column == 1) {
        int8_t high = droppedTable_[row][2];
        id |= high << 8;
    }
    return id;
}
