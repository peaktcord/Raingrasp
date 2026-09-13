#include "src/common/replay/data_harness.hpp"

#include <cstdio>

#include "src/common/game/items.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/spells.hpp"
#include "src/common/runtime.hpp"
#include "src/common/platform/desktop.hpp"

namespace data_harness {
namespace {

const char *text(const std::string &value) {
    return value.c_str();
}

void dumpItems() {
    std::printf("itemnames\tcount=%d\n", (int)Items::categoryCount_);
    for (int32_t index = 0; index < Items::categoryCount_; ++index) {
        std::printf("itemname\t%d\t%s\n", (int)index,
                    text(Items::categoryNames_[index]));
    }
    std::printf("items\tcount=%d\n", (int)Items::count_);
    for (int32_t index = 0; index < Items::count_; ++index) {
        std::printf("item\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)index,
                    text(Items::at(index).name), (int)Items::at(index).category,
                    (int)Items::at(index).tier, (int)Items::at(index).rating,
                    (int)Items::at(index).buyPrice, (int)Items::at(index).sellPrice,
                    (int)Items::at(index).effect);
    }
}

void dumpDropped() {
    std::printf("dropped\tcount=%d\n", (int)Items::droppedRows_);
    for (int32_t row = 0; row < Items::droppedRows_; ++row) {
        std::printf("drop\t%d", (int)row);
        for (int32_t column = 0; column < Items::droppedTable_[row].length(); ++column) {
            std::printf("\t%d", (int)Items::droppedTable_[row][column]);
        }
        std::printf("\n");
    }
}

void dumpSpells() {
    std::printf("spells\tcount=%d\n", (int)Spell::count_);
    for (int32_t index = 0; index < Spell::count_; ++index) {
        Spell *spell = Spell::all_[index];
        std::printf("spell\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", (int)index,
                    text(spell->name_), (int)spell->skill_,
                    (int)spell->magickaCost_, (int)spell->effect_,
                    (int)spell->target_, (int)spell->difficulty_, (int)spell->level_,
                    text(spell->description_));
    }
}

void dumpMonsters() {
    std::printf("monsters\tcount=%d\n", (int)Monster::typeCount_);
    for (int32_t index = 0; index < Monster::typeCount_; ++index) {
        std::printf("monster\t%d\t%s", (int)index,
                    text(Monster::typeNames_[index]));
        for (int32_t column = 0; column < 17; ++column) {
            std::printf("\t%d", (int)Monster::typeStats_[index][column]);
        }
        std::printf("\n");
    }
}

}

int run(int argc, char **argv, const game::Profile &profile) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    Resources::setRoot(argv[1]);
    profile.initStatics(platform::defaultContext());
    Items::loadItems(platform::defaultContext());
    Items::loadDropped(platform::defaultContext());
    Spell::load(platform::defaultContext());
    Monster::loadTypes(platform::defaultContext());
    dumpItems();
    dumpDropped();
    dumpSpells();
    dumpMonsters();
    std::fflush(stdout);
    return 0;
}

}
