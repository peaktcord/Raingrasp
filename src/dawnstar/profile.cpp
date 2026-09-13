#include "src/dawnstar/profile.hpp"

#include <memory>

#include "src/common/game/uistate.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/save_codec.hpp"
#include "src/dawnstar/variant.hpp"

namespace dawnstar {

namespace {

std::unique_ptr<game::Extension> newExtension() {
    return std::make_unique<Extension>();
}

std::unique_ptr<game::Variant> newVariant(Game &game) {
    return std::make_unique<Variant>(game);
}

const PlayerSaveCodec kSaveCodec;

const menuaction::Action kOptionsRows[] = {
    menuaction::STATS,     menuaction::INVENTORY,      menuaction::CLUE_LOG,
    menuaction::SKILLS,    menuaction::SPELLS,         menuaction::PORT_OPTIONS,
    menuaction::SAVE_GAME, menuaction::LOAD_GAME,      menuaction::HELP,
    menuaction::REVEAL_TRAITOR, menuaction::QUIT,
};

const commandflow::NavigationRule kNavigationRules[] = {
    {screens::NPC_QUESTION_RESPONSE, commandflow::Command::Ok, commandflow::Destination::NpcChoices},
    {uistate::SCREEN_CLASS_INFO, commandflow::Command::Ok, commandflow::Destination::CharacterSheet},
    {uistate::SCREEN_HELP_TOPIC, commandflow::Command::Any, commandflow::Destination::Help},
    {uistate::SCREEN_CREDITS, commandflow::Command::Any, commandflow::Destination::MainMenu},
};

const int8_t kSpriteLayout[4][22] = {
    {1, 5, 43, 48, 0, 2, 23, 7, 1, 3, 0, 64, 2, 18, 27, 3, 24, 19, 4, 0, 0, 0},
    {6, 10, 43, 49, 7, 2, 18, 8, 8, 3, 3, 84, 11, 17, 24, 10, 0, 80, 9, 0, 0, 0},
    {11, 25, 40, 50, 14, 3, 0, 0, -1, -1, 9, 29, 16, 11, 0, 15, 41, 41, 17, 0, 0, 0},
    {26, 40, 37, 50, 20, 3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
const int8_t kSpriteDrawMode[41][2] = {
    {0, 0}, {1, 0}, {0, 1}, {1, 2}, {0, 2}, {0, 1}, {1, 1}, {0, 2}, {1, 2}, {0, 2},
    {0, 0}, {1, 0}, {0, 0}, {1, 0}, {0, 0}, {2, 0}, {0, 0}, {2, 0}, {0, 0}, {2, 0},
    {1, 0}, {2, 0}, {1, 0}, {2, 0}, {2, 0}, {0, 0}, {1, 0}, {0, 0}, {1, 0}, {0, 0},
    {2, 0}, {0, 0}, {2, 0}, {0, 0}, {2, 0}, {1, 0}, {2, 0}, {1, 0}, {2, 0}, {1, 0},
    {0, 0}};
const uint8_t kSpriteParts[41][4] = {
    {false, false, false, false}, {true, true, false, false},  {false, false, true, false},
    {true, false, false, false},  {true, true, true, false},   {false, false, false, false},
    {true, false, false, false},  {false, false, true, false}, {false, true, false, false},
    {true, true, false, false},   {false, false, false, false},{false, false, false, false},
    {false, false, false, false}, {false, false, false, false},{false, false, false, false},
    {false, false, false, false}, {false, false, false, false},{false, false, false, false},
    {false, false, false, false}, {false, false, false, false},{false, false, false, false},
    {false, false, false, false}, {false, false, false, false},{false, false, false, false},
    {false, false, false, false}, {true, false, true, false},  {true, false, true, false},
    {true, false, true, false},   {true, false, true, false},  {true, false, true, false},
    {true, false, true, false},   {true, false, true, false},  {true, false, true, false},
    {true, true, false, false},   {true, true, false, false},  {true, true, false, false},
    {true, true, false, false},   {true, true, false, false},  {true, true, false, false},
    {true, true, false, false},   {false, false, false, false}};
const game::SpriteBand kSpriteBands[5] = {
    {1, 5, 0, 6, 5}, {6, 10, 1, 13, 12}, {11, 25, 3, 22, 21}, {26, 40, 2, 19, 18}, {41, 42, -1, 25, 24}};

game::Profile makeProfile() {
    game::Profile p;
    p.name = "dawnstar";
    p.saveCodec = &kSaveCodec;
    p.newExtension = &newExtension;

    const char *const ailments[8] = {"Frost Limbs", "Snow Mirage", "Blind",
                                     "Troll Thirst", "Glacier Curse", "Grievous Harm",
                                     "Terrified", "Winter Worn"};
    for (int32_t n = 0; n < 8; ++n) p.ailmentNames[n] = ailments[n];
    p.startingGold = 50;
    p.newGameStart = {9, 9, 1};
    p.campStart = {13, 6, 4};
    p.conjuredWeaponItem = 101;
    p.conjuredWeaponCategory = 15;
    p.secondUsableCategory = 0;
    p.knownSpellCheck = true;
    p.magickaLabel = "Magicka: ";
    p.sheetSpacer = "  ";

    p.spendClearsLevelUpMask = true;
    p.progressionRules.promoteAtThreshold = false;
    p.progressionRules.repeating = true;
    p.promoteOnAward = true;
    p.placementGuard = game::PlacementGuard::DestinationEmpty;
    p.visibleLayerOrder[0] = 1;
    p.visibleLayerOrder[1] = 4;
    p.visibleLayerOrder[2] = 2;
    p.warpEndsStrafe = true;
    p.placementRefreshesCanvas = true;
    p.recallRestoresFacing = true;
    p.restClearsCounters = true;

    p.menuBackground = 2510210;
    p.splashBackground = 2510210;
    p.splashTopY = 45;
    p.splashBottomY = 115;
    p.softKeyNegativeY = 194;
    p.softKeyPositiveY = 194;
    p.wrapFormRows = true;
    p.formPromptsAlwaysWrap = true;
    p.textBoxWrapMargin = 5;
    p.formMargin = 10;
    p.textWrap = textwrap::TextWrap::Slack;
    p.emptyListSelectsNone = false;
    p.bodyTextResetsScroll = true;

    p.spriteBands = kSpriteBands;
    p.spriteBandCount = 5;
    p.spriteLayout = kSpriteLayout;
    p.spriteDrawMode = kSpriteDrawMode;
    p.spriteParts = kSpriteParts;
    const game::ViewLayer layers[] = {
        game::ViewLayer::Walls,      game::ViewLayer::Entities,   game::ViewLayer::NpcPortrait,
        game::ViewLayer::Vitals,     game::ViewLayer::IconRow,    game::ViewLayer::MessageBox,
        game::ViewLayer::Effects,    game::ViewLayer::ErrorText,  game::ViewLayer::Minimap,
        game::ViewLayer::End};
    for (int32_t n = 0; n < (int32_t)(sizeof(layers) / sizeof(layers[0])); ++n) p.viewLayers[n] = layers[n];
    p.iceFloorOutsideCamp = true;
    p.hudLinesWrap = true;
    p.noMagickaWord = "magicka!";
    p.bloodOnEverySwing = false;
    p.aheadRefreshEveryTick = false;
    p.idleTalkKeyConsumesTick = true;
    p.loopStartSetsRunning = true;

    p.newVariant = &newVariant;
    p.initStatics = &dawnstar_init_statics;
    p.optionsRows = kOptionsRows;
    p.optionsRowCount = (int32_t)(sizeof(kOptionsRows) / sizeof(kOptionsRows[0]));
    p.navigationRules = kNavigationRules;
    p.navigationRuleCount =
        (int32_t)(sizeof(kNavigationRules) / sizeof(kNavigationRules[0]));
    p.creditsProgramming = "Marc Ilgen, Roland Kemp";
    p.firstRecordTable = 0;
    p.monsterKey = worldstate::MonsterKey::Coordinates;
    p.npcSpriteSlots = 26;
    const int32_t bands[5][2] = {{0, 7}, {7, 7}, {14, 6}, {20, 3}, {23, 3}};
    for (int32_t n = 0; n < 5; ++n) {
        p.monsterImageBands[n][0] = bands[n][0];
        p.monsterImageBands[n][1] = bands[n][1];
    }
    p.excludedMonsterImages = nullptr;
    p.excludedMonsterImageCount = 0;
    p.showProgressBeforeJob = true;
    p.newGameJob = game::NewGameJob::AfterName;
    p.characterFlow = game::CharacterFlow::PlaceAtIntroEnd;
    p.characterCreatedPrompt = "Character Created!\n \nPress 'Ok' to enter a name";
    p.characterCreatedSelects = false;
    p.nameFormCancels = false;
    p.introScreenCount = 2;
    p.introSecondScreen = screens::INTRODUCTION_SECOND;
    p.loadRefreshesSurroundings = false;
    p.loadFailureReturnsToSource = true;
    p.helpListPersists = true;
    p.rebuiltListsRestoreSelection = true;
    p.warpCheckAfterAnyItemAction = true;
    p.inventoryShowsGold = true;
    p.levelUpPromptBreaks = false;
    p.quitFormKeepsCancel = false;
    return p;
}

}

const game::Profile &profile() {
    static const game::Profile kProfile = makeProfile();
    return kProfile;
}

}
