#ifndef STORMHOLD_NPC_SCRIPT_HPP
#define STORMHOLD_NPC_SCRIPT_HPP

#include <optional>

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"
#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/world_state.hpp"

class Player;

namespace stormhold {

class Dungeon;

class NpcSystem {
public:
    static SharedArray<std::string> npcNames_;
    static SharedArray<int8_t> npcKind_;
    static SharedArray<int8_t> npcGridX_;
    static SharedArray<int8_t> npcGridY_;
    static SharedArray<SharedArray<std::string>> dialogue_;
    static SharedArray<int32_t> dialogueCounts_;
    static bool dialogueLoaded_;

    static void loadDialogue(platform::PlatformContext *context);
    static void loadDialogueFrom(platform::PlatformContext *context,
                                 const std::string &string);
    static bool wardenDue(const worldstate::NpcState &state, int32_t n);
    static void wardenArrive(worldstate::NpcState &state,
                             const worldstate::DungeonRegistry &dungeons);
    static void wardenLeave(worldstate::NpcState &state,
                            const worldstate::DungeonRegistry &dungeons);
    static bool isChampion(int32_t n);
    static int32_t npcAt(const worldstate::NpcState &state, int32_t n, int32_t n2);
    static int32_t giftValueFor(int32_t n, int32_t n2);
    static std::string tellRumor(Player *j2, int32_t n);
    static std::optional<std::string> interact(Player *j2, int32_t n, int32_t n2, int32_t n3);
    static bool isWardenAdjacent(Player *j2);
    static bool teaches(int32_t n, int32_t n2);
    static int32_t taughtSkill(int32_t n, int32_t n2);
    static void initializeStatics();
};

}
#endif
