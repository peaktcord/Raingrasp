#ifndef COMMON_GAME_GAME_HPP
#define COMMON_GAME_GAME_HPP

#include <memory>
#include <optional>
#include <vector>

#include "src/common/game/canvas_host.hpp"
#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/menuaction.hpp"
#include "src/common/game/profile.hpp"
#include "src/common/game/registered_application.hpp"
#include "src/common/game/ui_host.hpp"
#include "src/common/game/variant.hpp"
#include "src/common/game/widget_canvas.hpp"
#include "src/common/game/world.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"
#include "src/common/ui.hpp"
#include "src/common/save_records.hpp"

namespace platform { class PlatformContext; }

class GameCanvas;
class Player;
class UIWidget;

class Game : public RegisteredApplication,
               public game::World,
               public game::UiHost,
               public game::CanvasHost {
public:
    void startCanvasLoop(GameCanvas *canvas) override {
        if (canvasLoopStarter_ != nullptr) canvasLoopStarter_(canvas);
    }
    void showLevelUp() override;
    void showEndOfGame() override;
    void showOptions() override;

    using RegisteredApplication::exit;
    using RegisteredApplication::finalExit;

    game::WidgetCanvas *uiCanvas() override { return uiCanvas_; }
    UIWidget *currentUi() override { return currentUI_; }
    CommandListener *commandListener() override { return this; }
    void showDisplayable(game::DisplayTarget target) override { setCurrentDisplay(target); }
    game::DisplayTarget errorForm() override { return errorForm_; }
    void splashStarted(UIWidget *splash) override {
        if (splashStarter_ != nullptr) splashStarter_(splash);
    }
    bool switchRequested() const { return switchRequested_; }
    void requestSwitch() { switchRequested_ = true; }
    bool showingCarrierLogo() const override { return showCarrierLogo_; }
    bool showingSplash() const override { return showSplash_; }
    void setShowingCarrierLogo(bool on) override { showCarrierLogo_ = on; }
    void setShowingSplash(bool on) override { showSplash_ = on; }
    game::SplashArt splashArt() const override { return splashArt_; }
    void releaseSplashArt() override { splashArt_ = game::SplashArt(); }

    const game::Profile &profile() const override { return *profile_; }
    worldstate::WorldState &worldState() override { return worldState_; }
    platform::PlatformContext *platformContext() override { return platformContext_; }
    DungeonCore *dungeonAt(int32_t dungeonId) override { return dungeons_.atId(dungeonId); }
    void removeMonsterAt(int32_t dungeonId, int32_t x, int32_t y) override;
    int32_t gameAdvancementLevel(int32_t giftPoints) override {
        return variant_->gameAdvancementLevel(giftPoints);
    }
    void openAndRepopulateDungeons(int32_t level) override {
        variant_->openAndRepopulateDungeons(level);
    }
    void refreshAhead() override;
    Monster *combatTarget() override;

    game::Variant &variant() { return *variant_; }

    int32_t applicationState_ = 0;
    std::unique_ptr<Form> errorFormStorage_;
    std::unique_ptr<StringItem> errItemStorage_;
    Form *errorForm_ = nullptr;
    StringItem *errItem_ = nullptr;
    int32_t helperThreadState_ = 1;

    using HelperJobRunner = void (*)(Game *game, int32_t job);
    using CanvasLoopStarter = void (*)(GameCanvas *canvas);
    using SplashStarter = void (*)(UIWidget *splash);

    void setExecutionHooks(HelperJobRunner helperJobRunner,
                           CanvasLoopStarter canvasLoopStarter = nullptr,
                           SplashStarter splashStarter = nullptr) {
        helperJobRunner_ = helperJobRunner;
        canvasLoopStarter_ = canvasLoopStarter;
        splashStarter_ = splashStarter;
    }

    void startHelperJob(int32_t job);

    void runHelperJob(int32_t job);

    SharedArray<SharedArray<std::string>> monsterFilenames_;
    SharedArray<int32_t> attribIncr_ = SharedArray<int32_t>(3);
    Display *display_ = nullptr;
    game::SplashArt splashArt_;
    UIWidget *mainMenuUI_ = nullptr;
    UIWidget *newGameUI_ = nullptr;
    UIWidget *characterMainUI_ = nullptr;
    UIWidget *characterCreatedUI_ = nullptr;
    UIWidget *noSavedGameUI_ = nullptr;
    UIWidget *saveGameUI_ = nullptr;
    UIWidget *loadGameUI_ = nullptr;
    UIWidget *splashUI_ = nullptr;
    UIWidget *loadDungeonUI_ = nullptr;
    UIWidget *createGameUI_ = nullptr;
    SharedArray<UIWidget *> NPCChoicesUI_;
    UIWidget *GenericInfoUI_ = nullptr;
    UIWidget *OptionsUI_ = nullptr;
    UIWidget *InventoryUI_ = nullptr;
    UIWidget *InventoryItemUI_ = nullptr;
    UIWidget *SkillsListUI_ = nullptr;
    UIWidget *SpellsListUI_ = nullptr;
    UIWidget *SpellInfoUI_ = nullptr;
    UIWidget *LevelUpUI_ = nullptr;
    UIWidget *endOfGameUI_ = nullptr;
    UIWidget *confirmQuitUI_ = nullptr;
    UIWidget *helpUI_ = nullptr;
    UIWidget *portOptionsUI_ = nullptr;
    std::unique_ptr<GameCanvas> gameCanvasStorage_;
    GameCanvas *gameCanvas_ = nullptr;
    std::unique_ptr<game::WidgetCanvas> uiCanvasStorage_;
    game::WidgetCanvas *uiCanvas_ = nullptr;
    std::unique_ptr<Form> charNameTextFormStorage_;
    std::unique_ptr<StringItem> charNamePromptStorage_;
    std::unique_ptr<TextField> charNameFieldStorage_;
    Form *charNameTextForm_ = nullptr;
    SharedArray<std::string> helpStrings_ = SharedArray<std::string>(12);
    SharedArray<std::string> helpTitles_ = SharedArray<std::string>(12);
    std::string creditsString_;
    std::unique_ptr<Player> characterStorage_;
    Player *character_ = nullptr;
    std::unique_ptr<Player> pendingCharacterStorage_;
    Player *pendingCharacter_ = nullptr;
    worldstate::WorldState worldState_;
    worldstate::DungeonRegistry dungeons_;
    bool imgloadRunning_ = false;
    bool killThread_ = false;
    int8_t loadingDungeonId_ = 0;
    bool imgsLoaded_ = false;
    UIWidget *currentUI_ = nullptr;
    int32_t currentItemIndex_ = 0;
    int32_t currentSpellIndex_ = 0;
    bool switchRequested_ = false;
    bool showCarrierLogo_ = false;
    bool showSplash_ = false;
    std::unique_ptr<GameRandom> rng_;
    bool reloadGame_ = false;
    std::vector<std::unique_ptr<UIWidget>> ownedUiWidgets_;
    platform::PlatformContext *platformContext_ = nullptr;
    HelperJobRunner helperJobRunner_ = nullptr;
    CanvasLoopStarter canvasLoopStarter_ = nullptr;
    SplashStarter splashStarter_ = nullptr;

    Game(const game::Profile &profile, platform::PlatformContext *platformContext);
    ~Game() override;
    GameCanvas *createGameCanvas();
    UIWidget *makeOwnedUIWidget(int32_t layout, int32_t screenId);
    void startRegisteredApplication() override;
    void initSplash();
    void runAppload();

    static int64_t appLoadHoldMs() { return 1000; }
    void allocateGame();
    void allocateAllUIs();
    UIWidget *newInventoryUI();
    UIWidget *newPortOptionsUI(UIWidget *back);
    void refreshPortOptionsUI();
    UIWidget *newConfirmQuitUI(UIWidget *back);
    UIWidget *newHelpUI(UIWidget *back);
    void pauseApplication() override;
    void destroyApplication(bool bl) override;
    bool handleNavigationCommand(Command *command);
    void performMenuAction(menuaction::Action action, bool fromOptions);
    bool handleMenuCommand(Command *command);
    bool handleCharacterCommand(Command *command);
    bool handleInventoryCommand(Command *command);
    bool handleAbilitiesCommand(Command *command);
    void returnToNpcChoices(int32_t npc);
    bool handleHelpCommand(Command *command);
    void handleFormCommand(Command *command, Displayable *displayable);
    void commandAction(Command *command, Displayable *displayable) override;
    void commandAction1(Command *command, Displayable *displayable);
    void startPlay();
    void showExitScreen();
    bool loadGameState();
    bool saveGameState();
    void createErrorForm();
    void writeMasterListsToSave(SaveRecords *saveRecords);
    int32_t readMasterListRecords(SaveRecords *saveRecords, int32_t n);
    static SharedArray<int8_t> readBytesFromBinaryReader(BinaryReader *dataInputStream, int32_t n);
    static void writeBytesToBinaryWriter(BinaryWriter *dataOutputStream,
                                             const SharedArray<int8_t> &byArray, int32_t n);
    void loadCampMonsters();
    void runImageLoader();
    void runImageLoaderFor(const SharedArray<int8_t> &byArray);
    void unloadAllMonsterImages();
    UIWidget *newInventoryItemUI(int32_t n);
    UIWidget *newSkillsListUI();
    UIWidget *newSpellsListUI();
    UIWidget *newSpellInfoUI(int32_t n);
    UIWidget *newLevelUpUI(int32_t n);
    void setCurrentDisplay(game::DisplayTarget target);
    std::string describeScreenId(int32_t screenId) const;
    void loadMonsterFilenames();
    void displayError(const std::string &text);
    std::string unusedSaveName();
      std::optional<std::string> lastGoodSaveName();
    void cleanupSaveRecords();
    void checkDestroyed();

private:
    const game::Profile *profile_ = nullptr;
    std::unique_ptr<game::Variant> variant_;
};

#endif
