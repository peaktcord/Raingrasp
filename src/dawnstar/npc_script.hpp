#ifndef DAWNSTAR_NPC_SCRIPT_HPP
#define DAWNSTAR_NPC_SCRIPT_HPP

#include <optional>

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

class Player;

namespace dawnstar {

class NpcSystem {
public:
    static SharedArray<std::string> npcNames_;
    static SharedArray<int8_t> npcKind_;
    static SharedArray<int8_t> npcGridX_;
    static SharedArray<int8_t> npcGridY_;
    static SharedArray<SharedArray<std::string>> dialogue_;
    static SharedArray<int32_t> dialogueCounts_;
    static bool dialogueLoaded_;
    static SharedArray<SharedArray<int8_t>> stockLists_;
    static const int8_t kTraitorRumors[4][6];
    static const int8_t kClueTrue[24];
    static const int8_t kClueFalse[24];

    static void loadDialogue(platform::PlatformContext *context);
    static void loadDialogueFrom(platform::PlatformContext *context,
                                 const std::string &string);
    static bool isChampion(int32_t n);
    static bool isPeddler(int32_t n);
    static int32_t npcAt(int32_t n, int32_t n2);
    static int32_t giftValueFor(int32_t n, int32_t n2);
    static std::string tellRumor(Player *j2, int32_t n);
    static std::optional<std::string> interact(Player *j2, int32_t n, int32_t n2, int32_t n3);
    static bool teaches(int32_t n, int32_t n2);
    static int32_t taughtSkill(int32_t n, int32_t n2);
    static void initializeStatics();
};

}
#endif
