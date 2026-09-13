#include <cstdio>
#include <cstring>
#include <vector>
#include "src/common/game/commandflow.hpp"
#include "src/common/game/uistate.hpp"

namespace {
int failures = 0;
void check(bool value, const char *message) {
    if (!value) { std::printf("FAIL: %s\n", message); ++failures; }
}
struct Player {
    bool equipped = false;
    std::vector<int> calls;
    bool isEquipped(int) { return equipped; }
    void dropItem(int index) { calls.push_back(100 + index); }
    void equip(int index, bool enabled) { check(enabled, "equip enables slot"); calls.push_back(200 + index); equipped = true; }
    void unequipItem(int index) { calls.push_back(300 + index); equipped = false; }
    void learnSpell(int index) { calls.push_back(400 + index); }
    void useItemOnTarget(int index) { calls.push_back(500 + index); }
};
}

int main() {
    using namespace commandflow;
    // Every combination of optional rows must retain the same action identity.
    for (int mask = 0; mask < 16; ++mask) {
        const auto rows = itemRows(mask & 1, mask & 2, mask & 4, mask & 8);
        check(rows.at(0) == ItemAction::Drop, "Drop always first");
        check(rows.at(-1) == ItemAction::None && rows.at(rows.count) == ItemAction::None,
              "out-of-list selection performs no action");
        Player player;
        player.equipped = mask & 2;
        for (int i = 0; i < rows.count; ++i) executeItemAction(player, 7, rows.at(i));
        std::vector<int> expected{107};
        if (mask & 1) expected.push_back(mask & 2 ? 307 : 207);
        if (mask & 4) expected.push_back(407);
        if (mask & 8) expected.push_back(507);
        check(player.calls == expected, "visible rows execute the intended operations once, on the selected item");
        if (mask & 1)
            check(std::strcmp(rows.rows[1].label, mask & 2 ? "Unequip" : "Equip") == 0,
                  "equipment label agrees with offered action");
    }
    Player player;
    executeItemAction(player, 7, ItemAction::None);
    check(player.calls.empty(), "ignored selection has no player effects");

    const NavigationRule rules[] = {
        {700, Command::Ok, Destination::Next},
        {701, Command::Any, Destination::Game},
    };
    for (Command command : {Command::Ok, Command::Select, Command::Cancel, Command::Back, Command::Other}) {
        const auto guarded = navigate(700, command, false, rules, 2);
        check(guarded.recognized, "guarded screen consumes ignored commands");
        check(guarded.destination == (command == Command::Ok ? Destination::Next : Destination::None),
              "OK remains distinct from Select and Back");
        check(navigate(701, command, false, rules, 2).destination == Destination::Game,
              "any-command screen advances on unknown commands too");
    }
    check(navigate(701, Command::Cancel, true, rules, 2).destination == Destination::Back,
          "Cancel precedes even an any-command transition");
    check(navigate(999, Command::Cancel, true, rules, 2).destination == Destination::Back,
          "global Cancel also applies to unknown screens");
    check(!navigate(999, Command::Select, false, rules, 2).recognized, "unknown screen can reach another handler");
    check(navigate(uistate::SCREEN_STATS, Command::Ok, false, rules, 2).destination == Destination::Options,
          "stats returns to options");
    check(navigate(uistate::SCREEN_SKILL_INFO, Command::Ok, false, rules, 2).destination == Destination::Skills,
          "skill description returns to skills");

    SharedArray<std::string> names{"Strength", "Intelligence", "Willpower"};
    check(attributeChoice("Willpower", names) == 2, "level-up maps label to attribute, not visible row");
    check(attributeChoice("missing", names) == -1, "unknown label preserves legacy sentinel");
    SharedArray<int16_t> attributes{10, 20, 30};
    applyAttributeChoices(attributes, SharedArray<int32_t>{2, 0, 1});
    check(attributes[0] == 12 && attributes[1] == 21 && attributes[2] == 33,
          "three level-up choices receive 3, 2 and 1 points in choice order");
    applyAttributeChoices(attributes, SharedArray<int32_t>{0, 0, 0});
    check(attributes[0] == 18, "repeated choices accumulate in order");
    return failures ? 1 : 0;
}
