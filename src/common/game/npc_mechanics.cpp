#include "src/common/game/npc_mechanics.hpp"

#include "src/common/game/items.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/skills.hpp"
#include "src/common/game/smallhelpers.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/binary_io.hpp"

namespace npcmechanics {

bool loadDialogue(platform::PlatformContext *context, const std::string &path,
                  const SharedArray<int32_t> &counts, SharedArray<SharedArray<std::string>> &dialogue) {
    try {
        BinaryReader *dataInputStream = GameUtil::openResource(context, path);
        int32_t n1 = counts.length();
        dialogue = SharedArray<SharedArray<std::string>>(n1);
        int32_t n2 = 0;
        while (n2 < n1) {
            dialogue[n2] = smallhelpers::readNpcMessages(n2, counts[n2], dataInputStream);
            ++n2;
        }
        delete dataInputStream;
        return true;
    } catch (const std::exception &exception) {
        platform::writeLogLine(
            std::string("ERROR: failed to load the NPC and generic string tables: ") +
            exception.what());
        return false;
    }
}

int32_t giftValue(int32_t champion, int32_t item) {
    int32_t n1 = Items::stat(3, item);
    if (champion == 0) {
        return unsignedShiftRight32(n1, 6) & 3;
    }
    if (champion == 1) {
        return unsignedShiftRight32(n1, 4) & 3;
    }
    if (champion == 2) {
        return unsignedShiftRight32(n1, 2) & 3;
    }
    if (champion == 3) {
        return n1 & 3;
    }
    return 0;
}

std::string trainSkill(Player &player, int32_t skill, const SharedArray<std::string> &genericRow) {
    int16_t s1 = player.skills_[skill][0];
    if (s1 == 0) {
        player.skills_[skill][0] = 1;
        std::string string1 = genericRow[1];
        string1 = GameUtil::replace(string1, std::string("<TAG>"), Player::skillNames_[skill]);
        return string1;
    }
    player.skills_[skill][0] = (int16_t)(s1 + 1);
    std::string string2 = genericRow[2];
    SharedArray<std::string> stringArray1{Player::skillNames_[skill], std::to_string((int32_t)s1),
                                std::to_string((int32_t)(s1 + 1))};
    string2 = GameUtil::replace(string2, std::string("<TAG>"), stringArray1);
    return string2;
}

int32_t befriend(Player &player, worldstate::NpcState &state, int32_t champion) {
    if (state.befriendDone[champion] != 0) {
        return -1;
    }
    int32_t n9 = player.npcInteractionCheck(champion, 2);
    if (n9 == 0) {
        state.befriendDone[champion] = 1;
    } else if (n9 == 1) {
        player.awardSkillXp(skills::SPEECHCRAFT, 2);
    } else if (n9 == 2) {
        player.awardSkillXp(skills::SPEECHCRAFT, 5);
        state.aidPoints[champion] = (int16_t)(state.aidPoints[champion] + 1);
        state.befriendDone[champion] = 1;
    } else if (n9 == 3) {
        player.awardSkillXp(skills::SPEECHCRAFT, 8);
        state.aidPoints[champion] = (int16_t)(state.aidPoints[champion] + 1);
        state.befriendDone[champion] = 1;
    }
    state.interactionCount[champion] = (int16_t)(state.interactionCount[champion] + 1);
    return n9;
}

int32_t threaten(Player &player, worldstate::NpcState &state, int32_t champion,
                 int32_t strength) {
    if (state.threatenDone[champion] != 0) {
        return -1;
    }
    int32_t n14 = player.npcInteractionCheck(champion, 3);
    int32_t n13 = strength <= 1 ? 0 : 1;
    if (n14 == 0) {
        state.threatenDone[champion] = 2;
    } else if (n14 == 1) {
        player.awardSkillXp(skills::SPEECHCRAFT, 2);
        state.threatenDone[champion] = 2;
    } else if (n14 == 2) {
        player.awardSkillXp(skills::SPEECHCRAFT, 5);
        state.aidPoints[champion] = (int16_t)(state.aidPoints[champion] + 1);
        state.threatenDone[champion] = 1;
    } else if (n14 == 3) {
        player.awardSkillXp(skills::SPEECHCRAFT, 8);
        state.aidPoints[champion] = (int16_t)(state.aidPoints[champion] + 1);
        state.threatenDone[champion] = 1;
    }
    state.interactionCount[champion] = (int16_t)(state.interactionCount[champion] + 1);
    return n13;
}

int32_t acceptGift(Player &player, worldstate::NpcState &state, int32_t champion,
                   int32_t slot) {
    int32_t n19 = wrappingAbs(player.inventory_[slot]);
    int32_t n20 = giftValue(champion, n19);
    if (n20 > 0) {
        player.removeItem(slot);
        state.aidPoints[champion] = (int16_t)(state.aidPoints[champion] + n20);
        state.befriendDone[champion] = 0;
        state.threatenDone[champion] = 0;
    }
    return n20;
}

void recover(Player &player) {
    player.vitals_[2] = player.vitals_[3];
    player.vitals_[4] = player.vitals_[5];
}

}
