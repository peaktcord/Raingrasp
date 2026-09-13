#include "src/common/game/commandflow.hpp"
#include "src/common/game/uistate.hpp"

namespace commandflow {

Navigation navigate(int32_t screen, Command command, bool hasBack,
                    const NavigationRule *rules, std::size_t count) {
    if (command == Command::Cancel && hasBack) return {true, Destination::Back};
    static constexpr NavigationRule shared[] = {
        {uistate::SCREEN_STATS, Command::Ok, Destination::Options},
        {uistate::SCREEN_SKILL_INFO, Command::Ok, Destination::Skills},
        {uistate::SCREEN_NPC_TRAIN_RESPONSE, Command::Ok, Destination::NpcChoices},
        {uistate::SCREEN_NPC_GIVE_RESPONSE, Command::Ok, Destination::NpcChoices},
        {uistate::SCREEN_NPC_BEFRIEND_RESPONSE, Command::Ok, Destination::NpcChoices},
        {uistate::SCREEN_NPC_THREATEN_RESPONSE, Command::Ok, Destination::NpcChoices},
        {uistate::SCREEN_NPC_CURE_RESPONSE, Command::Ok, Destination::NpcChoices},
        {uistate::SCREEN_NPC_RECOVERY_RESPONSE, Command::Ok, Destination::NpcChoices},
    };
    for (const auto &rule : shared) {
        if (screen == rule.screen)
            return {true, command == rule.command ? rule.destination : Destination::None};
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto &rule = rules[i];
        if (screen == rule.screen)
            return {true, rule.command == Command::Any || command == rule.command
                              ? rule.destination : Destination::None};
    }
    return {};
}

ItemRows itemRows(bool equippable, bool equipped, bool learnable, bool usable) {
    ItemRows result;
    result.rows[result.count++] = {ItemAction::Drop, "Drop"};
    if (equippable)
        result.rows[result.count++] = {ItemAction::ToggleEquip, equipped ? "Unequip" : "Equip"};
    if (learnable) result.rows[result.count++] = {ItemAction::Learn, "Learn"};
    if (usable) result.rows[result.count++] = {ItemAction::Use, "Use"};
    return result;
}

int32_t attributeChoice(const std::string &label, const SharedArray<std::string> &names) {
    for (int32_t i = 0; i < names.length(); ++i)
        if (label == names[i]) return i;
    return -1;
}

void applyAttributeChoices(SharedArray<int16_t> &attributes, const SharedArray<int32_t> &choices) {
    for (int32_t i = 0; i < 3; ++i) {
        const int32_t attribute = choices[i];
        attributes[attribute] = (int16_t)(attributes[attribute] + 3 - i);
    }
}

}
