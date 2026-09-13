#ifndef COMMON_GAME_NPC_MECHANICS_HPP
#define COMMON_GAME_NPC_MECHANICS_HPP

#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

class Player;

namespace npcmechanics {

bool loadDialogue(platform::PlatformContext *context, const std::string &path,
                  const SharedArray<int32_t> &counts, SharedArray<SharedArray<std::string>> &dialogue);

int32_t giftValue(int32_t champion, int32_t item);

std::string trainSkill(Player &player, int32_t skill, const SharedArray<std::string> &genericRow);

int32_t befriend(Player &player, worldstate::NpcState &state, int32_t champion);

int32_t threaten(Player &player, worldstate::NpcState &state, int32_t champion,
                 int32_t strength);

int32_t acceptGift(Player &player, worldstate::NpcState &state, int32_t champion,
                   int32_t slot);

void recover(Player &player);

}

#endif
