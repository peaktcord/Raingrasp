#ifndef STORMHOLD_VARIANT_HPP
#define STORMHOLD_VARIANT_HPP

#include <memory>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/variant.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"
#include "src/stormhold/dungeon_gen.hpp"

namespace stormhold {

namespace screens {
constexpr int NPC_CHOICES_0 = 9;
constexpr int NPC_CHOICES_1 = 10;
constexpr int NPC_CHOICES_2 = 11;
constexpr int NPC_CHOICES_3 = 12;
constexpr int NPC_CHOICES_4 = 13;
constexpr int NPC_CHOICES_5 = 14;
constexpr int NPC_KILL_RESPONSE = 26;
constexpr int NPC_ENCHANT_WHAT = 27;
constexpr int NPC_ENCHANT_RESPONSE = 28;
constexpr int RETURN_TO_GAME = 30;
constexpr int RETURN_TO_GAME_PAUSED = 102;
constexpr int BACK_MESSAGE = 205;
constexpr int NPC_TAKE_WHAT = 350;
constexpr int NPC_TAKE_RESPONSE = 351;
constexpr int NPC_BLESS_RESPONSE = 352;
}

extern SharedArray<SharedArray<std::string>> itemEffectText;

void stormhold_init_statics(platform::PlatformContext *context);

class Variant : public game::Variant {
public:
    explicit Variant(Game &game);

    game::SplashArt loadSplashArt() override;
    void loadTables() override;
    void loadArt() override;
    render::Sprite *loadMonsterSprite(const std::string &name) override;
    void allocateScreens() override;
    void buildDungeons() override;
    void createNewGame() override;
    void resumeGame() override;
    void openAndRepopulateDungeons(int32_t group) override;
    int32_t gameAdvancementLevel(int32_t giftPoints) override;
    UIWidget *infoWidget(int32_t screenId) override;
    UIWidget *infoBox(int32_t screenId, const std::string &title, const std::string &body,
                      game::DisplayTarget back, game::DisplayTarget next) override;
    void refreshNpcAid(int32_t npc) override;
    const char *screenName(int32_t screenId) const override;
    std::string introductionText(int32_t page) override;
    void showEndOfGame() override;
    void showGameOver() override;
    bool handleCommand(Command *command) override;
    void openNpcScreen(GameCanvas &canvas, int32_t npc) override;

    DungeonGen dungeonGen_;
    SharedArray<SharedArray<int8_t>> dungeonGeometry_;

    UIWidget *npcHelloUI_ = nullptr;
    UIWidget *rumorsUI_ = nullptr;
    UIWidget *NPCGiveWhatUI_ = nullptr;
    UIWidget *NPCTrainWhatUI_ = nullptr;
    UIWidget *npcTakeResponseUI_ = nullptr;
    UIWidget *NPCTakeWhatUI_ = nullptr;
    UIWidget *npcBefriendUI_ = nullptr;
    UIWidget *npcThreatenUI_ = nullptr;
    UIWidget *npcTrainResponseUI_ = nullptr;
    UIWidget *npcKillUI_ = nullptr;
    UIWidget *NPCEnchantWhatUI_ = nullptr;
    UIWidget *npcGiveResponseUI_ = nullptr;
    UIWidget *npcBlessUI_ = nullptr;
    UIWidget *npcCureUI_ = nullptr;
    UIWidget *npcWarpUI_ = nullptr;
    UIWidget *npcRecoveryUI_ = nullptr;
    UIWidget *wardenSpeaksUI_ = nullptr;

    void handleNPCChoices(UIWidget *choicesScreen);
    UIWidget *newNpcResponseUI(UIWidget *h2, int32_t n, int32_t n2, int32_t n3, int32_t n4);
    UIWidget *newGiveWhat(int32_t n);
    UIWidget *newTrainWhat(int32_t n);
    UIWidget *newTakeWhatUI(int32_t n);
    UIWidget *newEnchantWhatUI(int32_t n);
    void showHelgaRumor();
    UIWidget *newWardenSpeaksUI(const std::string &string);
    UIWidget *newEndOfGameUI();
    UIWidget *newGameOverUI();

    Image *createImage(const std::string &string);

private:
    void loadGeometry();
    void loadHelpTitles();
    void loadHelpStrings();
    void openDungeonGroup(int32_t group);

    Game &game_;
};

inline Variant &variantOf(Game &game) { return static_cast<Variant &>(game.variant()); }
inline Variant &variantOf(Player &player) {
    return variantOf(*static_cast<Game *>(player.world_));
}

}
#endif
