#include "src/stormhold/profile.hpp"

#include <memory>

#include "src/common/game/uistate.hpp"
#include "src/stormhold/extension.hpp"
#include "src/stormhold/save_codec.hpp"
#include "src/stormhold/variant.hpp"

namespace stormhold {

namespace {

std::unique_ptr<game::Extension> newExtension() {
    return std::make_unique<Extension>();
}

std::unique_ptr<game::Variant> newVariant(Game &game) {
    return std::make_unique<Variant>(game);
}

const PlayerSaveCodec kSaveCodec;

const menuaction::Action kOptionsRows[] = {
    menuaction::STATS,     menuaction::INVENTORY, menuaction::SKILLS, menuaction::SPELLS,
    menuaction::PORT_OPTIONS, menuaction::SAVE_GAME, menuaction::LOAD_GAME,
    menuaction::HELP, menuaction::QUIT,
};

const commandflow::NavigationRule kNavigationRules[] = {
    {screens::NPC_ENCHANT_RESPONSE, commandflow::Command::Ok, commandflow::Destination::NpcChoices},
    {screens::NPC_TAKE_RESPONSE, commandflow::Command::Ok, commandflow::Destination::NpcChoices},
    {screens::NPC_BLESS_RESPONSE, commandflow::Command::Ok, commandflow::Destination::NpcChoices},
    {uistate::SCREEN_CLASS_INFO, commandflow::Command::Ok, commandflow::Destination::Next},
    {screens::NPC_KILL_RESPONSE, commandflow::Command::Any, commandflow::Destination::Game},
    {screens::RETURN_TO_GAME, commandflow::Command::Ok, commandflow::Destination::Game},
    {uistate::SCREEN_HELP_TOPIC, commandflow::Command::Any, commandflow::Destination::Back},
    {uistate::SCREEN_CREDITS, commandflow::Command::Any, commandflow::Destination::Back},
    {uistate::SCREEN_NO_SAVED_GAME, commandflow::Command::Any, commandflow::Destination::Back},
    {screens::BACK_MESSAGE, commandflow::Command::Any, commandflow::Destination::Back},
    {uistate::SCREEN_END_OF_GAME, commandflow::Command::Any, commandflow::Destination::Next},
    {uistate::SCREEN_GAME_OVER, commandflow::Command::Any, commandflow::Destination::Next},
};

const int32_t kExcludedMonsterImages[] = {4, 11, 18, 23, 30};

const int8_t kSpriteLayout[4][22] = {
    {1, 5, 31, 53, 0, 1, 40, -39, 1, 4, 13, -2, 3, 6, 71, 4, 30, 64, 2, 0, 0, 0},
    {6, 10, 31, 53, 7, 1, 27, -35, 8, 4, 1, 69, 11, 27, 66, 9, 33, 10, 10, 0, 0, 0},
    {11, 25, 31, 20, 14, 1, 0, 0, -1, -1, 2, 25, 15, 81, 8, 16, 9, 0, 17, 60, 57, 18},
    {26, 40, 31, 32, 21, 1, 0, 0, -1, -1, 43, 44, 22, 50, 25, 23, -36, 9, 24, -25, 44, 25}};
const int8_t kSpriteDrawMode[41][2] = {
    {0, 0}, {0, 0}, {0, 3}, {0, 3}, {0, 3}, {0, 2}, {0, 2}, {0, 3}, {0, 3}, {0, 3}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
const uint8_t kSpriteParts[41][4] = {
    {false, false, false, false}, {true, false, false, false}, {false, false, false, false},
    {false, false, true, false},  {true, false, true, false},   {false, false, false, false},
    {false, false, false, false}, {false, true, false, false},  {false, false, true, false},
    {false, true, true, false},   {true, false, false, false},  {false, true, false, false},
    {true, false, true, false},   {false, true, true, false},   {true, true, true, false},
    {true, false, false, false},  {false, true, false, false},  {true, false, true, false},
    {false, true, true, false},   {true, true, true, false},    {true, true, false, false},
    {true, true, false, false},   {true, true, false, false},   {true, true, true, false},
    {true, true, true, false},    {false, false, false, false}, {false, false, false, false},
    {false, false, false, false}, {false, false, false, false}, {false, false, false, false},
    {false, false, false, false}, {false, false, false, false}, {false, false, false, false},
    {false, false, false, false}, {false, false, false, false}, {false, true, false, true},
    {false, true, false, true},   {true, false, false, true},   {true, false, true, false},
    {false, true, true, false},   {false, false, false, false}};
const game::SpriteBand kSpriteBands[5] = {
    {1, 5, 0, 6, 5}, {6, 10, 1, 13, 12}, {11, 25, 2, 20, 19}, {26, 40, 3, 27, 26}, {41, 41, -1, 32, 31}};

game::Profile makeProfile() {
    game::Profile p;
    p.name = "stormhold";
    p.saveCodec = &kSaveCodec;
    p.newExtension = &newExtension;

    const char *const ailments[8] = {"Stone Blood", "Delusions", "Blind",
                                     "Vampirism", "Mana Burn", "Grievous Harm",
                                     "Terrified", "Haunted"};
    for (int32_t n = 0; n < 8; ++n) p.ailmentNames[n] = ailments[n];
    p.startingGold = 0;
    p.newGameStart = {9, 10, 1};
    p.campStart = {12, 14, 1};
    p.conjuredWeaponItem = 109;
    p.conjuredWeaponCategory = 17;
    p.secondUsableCategory = 15;
    p.knownSpellCheck = false;
    p.magickaLabel = "Magic: ";
    p.sheetSpacer = "";

    p.spendClearsLevelUpMask = false;
    p.progressionRules.promoteAtThreshold = true;
    p.progressionRules.repeating = false;
    p.promoteOnAward = false;
    p.placementGuard = game::PlacementGuard::PathClear;
    p.visibleLayerOrder[0] = 2;
    p.visibleLayerOrder[1] = 4;
    p.visibleLayerOrder[2] = 1;
    p.warpEndsStrafe = false;
    p.placementRefreshesCanvas = false;
    p.recallRestoresFacing = false;
    p.restClearsCounters = false;

    p.menuBackground = 11429934;
    p.splashBackground = 0;
    p.splashTopY = 20;
    p.splashBottomY = 100;
    p.softKeyNegativeY = 192;
    p.softKeyPositiveY = 195;
    p.wrapFormRows = false;
    p.formPromptsAlwaysWrap = false;
    p.textBoxWrapMargin = 10;
    p.formMargin = 15;
    p.textWrap = textwrap::TextWrap::Words;
    p.emptyListSelectsNone = true;
    p.bodyTextResetsScroll = false;

    p.spriteBands = kSpriteBands;
    p.spriteBandCount = 5;
    p.spriteLayout = kSpriteLayout;
    p.spriteDrawMode = kSpriteDrawMode;
    p.spriteParts = kSpriteParts;
    const game::ViewLayer layers[] = {
        game::ViewLayer::Walls,      game::ViewLayer::Objects,    game::ViewLayer::NpcPortrait,
        game::ViewLayer::Monsters,   game::ViewLayer::Vitals,     game::ViewLayer::MessageBox,
        game::ViewLayer::IconRow,    game::ViewLayer::Effects,    game::ViewLayer::ErrorText,
        game::ViewLayer::Minimap,    game::ViewLayer::End};
    for (int32_t n = 0; n < (int32_t)(sizeof(layers) / sizeof(layers[0])); ++n) p.viewLayers[n] = layers[n];
    p.iceFloorOutsideCamp = false;
    p.hudLinesWrap = false;
    p.noMagickaWord = "magic!";
    p.bloodOnEverySwing = true;
    p.aheadRefreshEveryTick = true;
    p.idleTalkKeyConsumesTick = false;
    p.loopStartSetsRunning = false;

    p.newVariant = &newVariant;
    p.initStatics = &stormhold_init_statics;
    p.optionsRows = kOptionsRows;
    p.optionsRowCount = (int32_t)(sizeof(kOptionsRows) / sizeof(kOptionsRows[0]));
    p.navigationRules = kNavigationRules;
    p.navigationRuleCount =
        (int32_t)(sizeof(kNavigationRules) / sizeof(kNavigationRules[0]));
    p.creditsProgramming = "Marc Ilgen";
    p.firstRecordTable = 1;
    p.monsterKey = worldstate::MonsterKey::Uid;
    p.npcSpriteSlots = 33;
    const int32_t bands[5][2] = {{0, 7}, {7, 7}, {14, 7}, {21, 7}, {28, 5}};
    for (int32_t n = 0; n < 5; ++n) {
        p.monsterImageBands[n][0] = bands[n][0];
        p.monsterImageBands[n][1] = bands[n][1];
    }
    p.excludedMonsterImages = kExcludedMonsterImages;
    p.excludedMonsterImageCount =
        (int32_t)(sizeof(kExcludedMonsterImages) / sizeof(kExcludedMonsterImages[0]));
    p.showProgressBeforeJob = false;
    p.newGameJob = game::NewGameJob::AtMenu;
    p.characterFlow = game::CharacterFlow::PendingAtClassSelect;
    p.characterCreatedPrompt = "Character Created!\n \nPress 'select' to enter a name";
    p.characterCreatedSelects = true;
    p.nameFormCancels = true;
    p.introScreenCount = 1;
    p.introSecondScreen = 0;
    p.loadRefreshesSurroundings = true;
    p.loadFailureReturnsToSource = false;
    p.helpListPersists = false;
    p.rebuiltListsRestoreSelection = false;
    p.warpCheckAfterAnyItemAction = false;
    p.inventoryShowsGold = false;
    p.levelUpPromptBreaks = true;
    p.quitFormKeepsCancel = true;
    return p;
}

}

const game::Profile &profile() {
    static const game::Profile kProfile = makeProfile();
    return kProfile;
}

}
