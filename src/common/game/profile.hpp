#ifndef COMMON_GAME_PROFILE_HPP
#define COMMON_GAME_PROFILE_HPP

#include <memory>

#include "src/common/game/commandflow.hpp"
#include "src/common/game/menuaction.hpp"
#include "src/common/game/progression.hpp"
#include "src/common/game/textwrap.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/runtime.hpp"

class Game;

namespace game {

class Extension;
class SaveCodec;
class Variant;

struct StartPosition {
    int8_t x = 0;
    int8_t y = 0;
    int8_t facing = 0;
};

enum class PlacementGuard {
    DestinationEmpty,
    PathClear,
};

struct SpriteBand {
    int8_t lo = 0;
    int8_t hi = 0;
    int8_t row = -1;
    int8_t farSprite = -1;
    int8_t midSprite = -1;
};

enum class ViewLayer {
    End,
    Walls,
    Entities,
    Objects,
    Monsters,
    NpcPortrait,
    Vitals,
    IconRow,
    MessageBox,
    Effects,
    ErrorText,
    Minimap,
};

enum class NewGameJob {
    AfterName,
    AtMenu,
};

enum class CharacterFlow {
    PlaceAtIntroEnd,
    PendingAtClassSelect,
};

struct Profile {
    const char *name = "";

    const SaveCodec *saveCodec = nullptr;

    std::unique_ptr<Extension> (*newExtension)() = nullptr;

    std::unique_ptr<Variant> (*newVariant)(Game &game) = nullptr;

    void (*initStatics)(platform::PlatformContext *context) = nullptr;

    const char *ailmentNames[8] = {};

    int32_t startingGold = 0;

    StartPosition newGameStart;
    StartPosition campStart;

    int32_t conjuredWeaponItem = 0;
    int8_t conjuredWeaponCategory = 0;

    int8_t secondUsableCategory = 0;

    bool knownSpellCheck = false;

    const char *magickaLabel = "";
    const char *sheetSpacer = "";

    bool spendClearsLevelUpMask = false;

    progression::Rules progressionRules;
    bool promoteOnAward = false;

    PlacementGuard placementGuard = PlacementGuard::DestinationEmpty;

    int32_t visibleLayerOrder[3] = {1, 4, 2};

    bool warpEndsStrafe = false;

    bool placementRefreshesCanvas = false;

    bool recallRestoresFacing = false;

    bool restClearsCounters = false;

    int32_t menuBackground = 0;

    int32_t splashBackground = 0;
    int32_t splashTopY = 0;
    int32_t splashBottomY = 0;

    int32_t softKeyNegativeY = 0;
    int32_t softKeyPositiveY = 0;

    bool wrapFormRows = false;

    bool formPromptsAlwaysWrap = false;

    int32_t textBoxWrapMargin = 0;
    int32_t formMargin = 0;

    textwrap::TextWrap textWrap = textwrap::TextWrap::Slack;

    bool emptyListSelectsNone = false;

    bool bodyTextResetsScroll = false;

    const SpriteBand *spriteBands = nullptr;
    int32_t spriteBandCount = 0;
    const int8_t (*spriteLayout)[22] = nullptr;
    const int8_t (*spriteDrawMode)[2] = nullptr;
    const uint8_t (*spriteParts)[4] = nullptr;

    ViewLayer viewLayers[12] = {ViewLayer::End};

    bool iceFloorOutsideCamp = false;

    bool hudLinesWrap = false;

    const char *noMagickaWord = "";

    bool bloodOnEverySwing = false;

    bool aheadRefreshEveryTick = false;

    bool idleTalkKeyConsumesTick = false;

    bool loopStartSetsRunning = false;

    const menuaction::Action *optionsRows = nullptr;
    int32_t optionsRowCount = 0;

    const commandflow::NavigationRule *navigationRules = nullptr;
    int32_t navigationRuleCount = 0;

    const char *creditsProgramming = "";

    int32_t firstRecordTable = 0;
    worldstate::MonsterKey monsterKey = worldstate::MonsterKey::Coordinates;

    int32_t npcSpriteSlots = 0;

    int32_t monsterImageBands[5][2] = {};
    const int32_t *excludedMonsterImages = nullptr;
    int32_t excludedMonsterImageCount = 0;

    bool showProgressBeforeJob = false;

    NewGameJob newGameJob = NewGameJob::AfterName;

    CharacterFlow characterFlow = CharacterFlow::PlaceAtIntroEnd;

    const char *characterCreatedPrompt = "";
    bool characterCreatedSelects = false;

    bool nameFormCancels = false;

    int32_t introScreenCount = 1;
    int32_t introSecondScreen = 0;

    bool loadRefreshesSurroundings = false;

    bool loadFailureReturnsToSource = false;

    bool helpListPersists = false;

    bool rebuiltListsRestoreSelection = false;

    bool warpCheckAfterAnyItemAction = false;

    bool inventoryShowsGold = false;

    bool levelUpPromptBreaks = false;

    bool quitFormKeepsCancel = false;
};

}

#endif
