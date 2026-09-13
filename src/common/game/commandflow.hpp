#pragma once

#include <array>
#include <cstddef>
#include "src/common/runtime.hpp"

namespace commandflow {

enum class Command { Select, Ok, Cancel, Back, Other, Any };
enum class Destination { None, Back, Next, Game, Options, Skills, Help, MainMenu, CharacterSheet, NpcChoices };
struct NavigationRule {
    int32_t screen;
    Command command;
    Destination destination;
};
struct Navigation {
    bool recognized = false;
    Destination destination = Destination::None;
};

Navigation navigate(int32_t screen, Command command, bool hasBack,
                    const NavigationRule *rules, std::size_t count);

enum class ItemAction { None = -1, Drop, ToggleEquip, Learn, Use };
struct ItemRow {
    ItemAction action = ItemAction::None;
    const char *label = "";
};
struct ItemRows {
    std::array<ItemRow, 4> rows{};
    int32_t count = 0;
    ItemAction at(int32_t row) const {
        return row >= 0 && row < count ? rows[row].action : ItemAction::None;
    }
};
ItemRows itemRows(bool equippable, bool equipped, bool learnable, bool usable);

template <class Player>
void executeItemAction(Player &player, int32_t index, ItemAction action) {
    switch (action) {
        case ItemAction::Drop: player.dropItem(index); break;
        case ItemAction::ToggleEquip:
            if (player.isEquipped(index)) player.unequipItem(index);
            else player.equip(index, true);
            break;
        case ItemAction::Learn: player.learnSpell(index); break;
        case ItemAction::Use: player.useItemOnTarget(index); break;
        case ItemAction::None: break;
    }
}

int32_t attributeChoice(const std::string &label, const SharedArray<std::string> &names);
void applyAttributeChoices(SharedArray<int16_t> &attributes, const SharedArray<int32_t> &choices);

}
