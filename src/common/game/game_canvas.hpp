#ifndef COMMON_GAME_GAME_CANVAS_HPP
#define COMMON_GAME_GAME_CANVAS_HPP

#include <memory>

#include "src/common/game/canvas_host.hpp"
#include "src/common/game/profile.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"
#include "src/common/render/sprite.hpp"

class Monster;
class Player;

namespace game {
class Extension;
}

class GameCanvas : public Canvas {
public:
    static Font *hudFont_;
    static Font *bigFont_;
    static Font *wideCompassFont_;
    static const int32_t kWallLookup[5][6][4];
    static const char kIconLabels[6];
    static const char kKeypadLabels[6];
    const char *iconLabels() const;
    static const char kFacingChars[5];
    static const int32_t kHudIconRows[3][3];

    static Image *floorImage_;
    static Image *floorIceImage_;
    static Image *wallRightImage_;
    static Image *wallInnerImage_;
    static Image *gateImage_;
    static SharedArray<render::Sprite *> npcSprites_;
    static SharedArray<render::Sprite *> chestSprites_;
    static SharedArray<render::Sprite *> bagSprites_;
    static SharedArray<render::Sprite *> crystalSprites_;
    static SharedArray<Image *> effectImages_;
    static Image *hudIconAtlas_;
    static SharedArray<Image *> hudIcons_;
    static Image *hudPanelImage_;

    game::CanvasHost *game_ = nullptr;
    const game::Profile *profile_ = nullptr;
    int32_t gameAction_ = 0;
    int32_t prevGameAction_ = 0;
    bool loopStarted_ = false;
    bool paused_ = false;
    bool running_ = false;
    bool killRequested_ = false;
    Player *player_ = nullptr;
    int8_t screenState_ = 0;
    bool stateChanged_ = false;
    int8_t campState_ = 0;
    int64_t campStartedAt_ = 0;
    int64_t diedAt_ = 0;
    int64_t lastAttackMs_ = 0;
    int64_t lastCastMs_ = 0;
    bool strafe_ = false;
    int32_t pendingMove_ = 0;
    bool monsterAhead_ = false;
    bool chestAhead_ = false;
    int32_t npcAhead_ = -1;
    bool npcTileAhead_ = false;
    SharedArray<SharedArray<int8_t>> mapGrid7_;
    SharedArray<SharedArray<int8_t>> mapGrid17_;
    int32_t mapMode_ = 1;
    bool mapDirty_ = false;
    bool showBloodFx_ = false;
    bool showMonsterSpellFx_ = false;
    bool showSelfSpellFx_ = false;
    int32_t hudLayout_ = 0;
    bool wantAttack_ = false;
    bool wantTalk_ = false;
    bool wantAction_ = false;
    bool wantCamp_ = false;
    bool wantOptions_ = false;
    bool wantCast_ = false;
    bool wantReadySpell_ = false;
    bool messageVisible_ = false;
    int64_t messageShownAt_ = 0;
    SharedArray<std::string> messageLines_;
    int32_t messagePriority_ = 0;
    bool repaintEnabled_ = true;
    bool actedThisTick_ = false;
    bool announceDungeon_ = false;
    bool campBlocked_ = false;
    static SharedArray<std::string> msgCannotCamp_;
    static SharedArray<std::string> msgNoSpells_;
    static SharedArray<std::string> msgNoMagicka_;
    static SharedArray<std::string> msgNoMonster_;
    static SharedArray<std::string> msgRestDisturbed_;
    static SharedArray<std::string> msgRestComplete_;
    static SharedArray<std::string> msgCreatureDead_;
    static SharedArray<std::string> msgCreatureAttacks_;
    static SharedArray<std::string> msgChest_;
    static SharedArray<std::string> msgChestLocked_;
    static SharedArray<std::string> msgInventoryFull_;
    static SharedArray<std::string> msgFoundItem_;
    static SharedArray<std::string> msgSeveralItems_;
    std::unique_ptr<Monster> combatMonsterStorage_;
    Monster *combatMonster_ = nullptr;
    int64_t lastTickMs_ = 0;
    bool showError_ = false;
    std::string errorText_ = std::string();

    explicit GameCanvas(game::CanvasHost *game);
    ~GameCanvas() override;

    game::Extension &ext();
    const game::Extension &ext() const;

    void paint(Graphics *graphics) override;
    void drawDeathScreen(Graphics *graphics);
    void drawCampScreen(Graphics *graphics);
    void drawGameView(Graphics *graphics);
    void drawErrorText(Graphics *graphics);
    void drawEffects(Graphics *graphics);
    void drawFloorAndWalls(Graphics *graphics);
    int32_t wallSlice(int32_t kind, int32_t depth, int32_t side);
    void showErrorScreen();
    static const char *viewLayerName(game::ViewLayer layer);
    static int32_t residentSpriteCount();
    std::string paintBreadcrumb() const;
    std::string failureContext() const;
    game::ViewLayer paintLayer_ = game::ViewLayer::End;
    // paintLayer_ only means anything while a frame is actually in flight.  A
    // completed frame leaves it holding the last layer of the profile's list,
    // so without this flag every failure outside paint reports that layer and
    // sends the reader to the wrong code.
    bool painting_ = false;
    class PaintScope;
    void drawSlots(Graphics *graphics, bool objects, bool monsters);
    void drawMonsterNear(Graphics *graphics, int32_t type, int32_t modeOverride);
    void drawMonsterMid(Graphics *graphics, int32_t sprite, int32_t slot);
    void drawMonsterFar(Graphics *graphics, int32_t sprite, int32_t slot);
    void drawFrame(Graphics *graphics, render::Sprite *sprite, int32_t frame, int32_t frames,
                   int32_t x, int32_t y, int32_t manipulation = 0);
    const game::SpriteBand *spriteBandOf(int32_t type) const;
    void drawVitals(Graphics *graphics);
    void drawMessageBox(Graphics *graphics);
    void drawIconRow(Graphics *graphics);
    void drawCompassAndMap(Graphics *graphics);
    int32_t hudLayoutFor();

    enum class LineRule {
        Whole,
        FirstSpace,
        Words,
    };
    SharedArray<std::string> messageLinesFor(const std::string &text, LineRule rule);
    bool showMessage(SharedArray<std::string> stringArray, int32_t n);
    void postMessage(SharedArray<std::string> lines, int32_t priority, int64_t nowMs);
    void refreshNpcAhead();
    void refreshMonsterAhead();
    void refreshChestAhead();

    void keyPressed(int32_t n) override;
    void keyReleased(int32_t n) override;
    void stopLoop();
    void startLoop();

    enum class TickStatus {
        Ran,
        Paused,
        Stopped,
        Failed,
    };

    TickStatus tick();

    int64_t pacingDelayMs(TickStatus status) const;

    bool tickStateInitialised_ = false;
    int64_t tickStartMs_ = 0;
    int64_t prevTickStartMs_ = 0;
    int64_t tickDeltaMs_ = 0;
    int64_t perSecondAccumMs_ = 0;
    void dispatchPendingAction(int64_t l);
    void doAttack(int64_t l);
    void handleMonsterDeath();
    void doMove();
    void doCast(int64_t l);
    void doReadySpell(int64_t l);
    void doTalk(int64_t l);
    SharedArray<std::string> hudStrings();
    void openOptions();
    void doCamp(int64_t l);
    void tickTimers(int64_t l, int64_t l2);
    void pauseForUi();
    void resume();
    void showNotify() override;
    bool campAmbushRoll();
    void tickRegen(int64_t l);
    void tickPerSecond();
    static void initializeStatics(const game::Profile &profile);
};

#endif
