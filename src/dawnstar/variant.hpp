#ifndef DAWNSTAR_VARIANT_HPP
#define DAWNSTAR_VARIANT_HPP

#include <string>
#include <unordered_map>

#include "src/common/game/game.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/variant.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

namespace dawnstar {

namespace screens {
constexpr int NPC_CHOICES_0 = 9;
constexpr int NPC_CHOICES_1 = 10;
constexpr int NPC_CHOICES_2 = 11;
constexpr int NPC_CHOICES_3 = 12;
constexpr int NPC_CHOICES_4 = 13;
constexpr int NPC_CHOICES_5 = 14;
constexpr int NPC_CHOICES_6 = 15;
constexpr int NPC_CHOICES_7 = 16;
constexpr int NPC_CHOICES_8 = 17;
constexpr int NPC_QUESTION_RESPONSE = 26;
constexpr int NPC_QUESTION_WHAT = 27;
constexpr int NPC_QUESTION_WHOM = 28;
constexpr int NPC_TRAVEL = 29;
constexpr int NPC_BUY_WHAT = 50;
constexpr int NPC_BUY_RESPONSE = 51;
constexpr int NPC_SELL_WHAT = 52;
constexpr int NPC_SELL_RESPONSE = 53;
constexpr int NPC_CONFIRM_SALE = 54;
constexpr int CLUE_LIST = 60;
constexpr int CLUE_INFO = 61;
constexpr int REVEAL_CONFIRM = 65;
constexpr int REVEAL_WHOM = 66;
constexpr int REVEAL_RESULT = 67;
constexpr int REVEAL_INTRO = 68;
constexpr int CAMP_CONFIRM = 69;
constexpr int INTRODUCTION_SECOND = 102;
constexpr int REGISTRATION_EXIT = 410;
}

extern SharedArray<std::string> itemEffectText;

void dawnstar_init_statics(platform::PlatformContext *context);

class Variant : public game::Variant {
public:
    explicit Variant(Game &game) : game_(game) {}

    game::SplashArt loadSplashArt() override;
    void loadTables() override;
    void loadArt() override;
    render::Sprite *loadMonsterSprite(const std::string &name) override;
    void allocateScreens() override;
    void buildDungeons() override;
    void createNewGame() override;
    void resumeGame() override;
    void openAndRepopulateDungeons(int32_t level) override;
    int32_t gameAdvancementLevel(int32_t giftPoints) override;
    void afterLoad() override;
    UIWidget *infoWidget(int32_t screenId) override;
    UIWidget *infoBox(int32_t screenId, const std::string &title, const std::string &body,
                      game::DisplayTarget back, game::DisplayTarget next) override;
    void refreshNpcAid(int32_t npc) override;
    const char *screenName(int32_t screenId) const override;
    std::string introductionText(int32_t page) override;
    void showEndOfGame() override;
    void showGameOver() override;
    bool performMenuAction(menuaction::Action action) override;
    bool handleCommand(Command *command) override;
    void openNpcScreen(GameCanvas &canvas, int32_t npc) override;

    UIWidget *NPCQuestionWhatUI_ = nullptr;
    UIWidget *NPCQuestionWhomUI_ = nullptr;
    UIWidget *NPCGiveWhatUI_ = nullptr;
    UIWidget *NPCTrainWhatUI_ = nullptr;
    UIWidget *NPCSellWhatUI_ = nullptr;
    UIWidget *NPCSellSureUI_ = nullptr;
    UIWidget *NPCBuyWhatUI_ = nullptr;
    UIWidget *NPCWarpUI_ = nullptr;
    UIWidget *WarpWhereUI_ = nullptr;
    UIWidget *ClueUI_ = nullptr;
    UIWidget *RevealUI_ = nullptr;
    int32_t currentQWhat_ = 0;
    int32_t currentQWhom_ = 0;

    void handleNPCChoices(UIWidget *choicesScreen);
    void handleNPCAction(int32_t n, int32_t n2, int32_t n3, int32_t n4);
    UIWidget *newGiveWhat(int32_t n);
    UIWidget *newSellWhat(int32_t n);
    UIWidget *newBuyWhat(int32_t n);
    UIWidget *newTrainWhat(int32_t n);
    UIWidget *newWarpWhere();
    void newClueLogUI(int32_t n);
    UIWidget *newRevealUI();
    UIWidget *newRevealWhomUI();
    UIWidget *newEndOfGameUI();
    UIWidget *newGameOverUI();

    void createImageFromFile();
    Image *createImage(const std::string &string);

private:
    void loadHelpStrings();
    int32_t nextInt(int32_t n);

    Game &game_;
    std::unordered_map<std::string, Image *> imageIndex_;
};

inline Variant &variantOf(Game &game) { return static_cast<Variant &>(game.variant()); }
inline Variant &variantOf(Player &player) {
    return variantOf(*static_cast<Game *>(player.world_));
}

}
#endif
