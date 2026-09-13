// Drives the public CommandListener entry point with real widgets and players.
// Build separately against each game; raw IDs below intentionally characterize
// the shipped protocol independently of the constants used by the controllers.
#include <cstdio>
#include "src/common/platform/desktop.hpp"
#include "src/common/replay/headless.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/menuaction.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#ifdef COMMAND_GAME_DAWNSTAR
#include "src/dawnstar/dungeon.hpp"
#include "src/dawnstar/variant.hpp"
#include "src/dawnstar/profile.hpp"
#else
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/variant.hpp"
#include "src/stormhold/profile.hpp"
#endif

#ifdef COMMAND_GAME_DAWNSTAR
using namespace dawnstar;
#else
using namespace stormhold;
#endif

namespace {
int failures = 0;
int jobSeen = -1, screenAtJob = -1;
void check(bool value, const char *message) {
    if (!value) { std::printf("FAIL: %s\n", message); ++failures; }
}
UIWidget *current(Game *game) { return game->currentUI_; }
void recordJob(Game *game, int32_t job) {
    jobSeen = job;
    screenAtJob = current(game) ? current(game)->screenId_ : -1;
}
void noCanvasLoop(GameCanvas *) {}
void noSplashLoop(UIWidget *) {}
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/command-flow");
    auto *context = platform::defaultContext();
#ifdef COMMAND_GAME_DAWNSTAR
    dawnstar_init_statics(context);
#else
    stormhold_init_statics(context);
#endif
    auto *ok = UIWidget::cmdOk_;
    auto *select = UIWidget::cmdSelect_;
    auto *cancel = UIWidget::cmdCancel_;
    Game game(profile(), context);
    game.setExecutionHooks(headless::runJobInline<Game>, noCanvasLoop, noSplashLoop);
    game.startApplication();
    check(game.mainMenuUI_ != nullptr && game.OptionsUI_ != nullptr, "boot allocated command screens");
    if (failures) return 1;
    game.setExecutionHooks(recordJob, noCanvasLoop, noSplashLoop);
    Command other("Unrecognized", 3, 0);

    auto *stats = game.makeOwnedUIWidget(4, 32);
    stats->setupTextBox("Stats", "Command fixture");
    stats->backTarget_ = game.mainMenuUI_;
    game.setCurrentDisplay(stats);
    game.commandAction(select, nullptr);
    check(current(&game) == stats, "Stats ignores Select");
    game.commandAction(ok, nullptr);
    check(current(&game) == game.OptionsUI_, "Stats OK returns to options");
    game.setCurrentDisplay(stats);
    game.commandAction(cancel, nullptr);
    check(current(&game) == game.mainMenuUI_, "Cancel takes back target before screen-specific handling");

    auto *message = game.makeOwnedUIWidget(4, 206);
    message->setupTextBox("Help", "Command fixture");
    message->backTarget_ = game.OptionsUI_;
    game.setCurrentDisplay(message);
    game.commandAction(&other, nullptr);
#ifdef COMMAND_GAME_DAWNSTAR
    check(current(&game) == game.helpUI_, "Dawnstar topic returns to shared Help widget");
#else
    check(current(&game) == game.OptionsUI_, "Stormhold topic follows its back target");
#endif

    game.characterStorage_ = std::make_unique<Player>(&game);
    game.character_ = game.characterStorage_.get();
    game.character_->initFromClass(0);
    auto *dungeon = game.dungeons_.emplace<Dungeon>(0, game.dungeons_, game.worldState_);
    dungeon->id_ = 1;
    dungeon->width_ = dungeon->height_ = 4;
    dungeon->tiles_ = makeSharedArray2D<int8_t>(4, 4);
    game.character_->dungeonId_ = 1;
    game.character_->gridX_ = game.character_->gridY_ = 1;
    check(game.character_->addItem(1, 0, 0), "add first fixture item");
    check(game.character_->addItem(2, 0, 0), "add second fixture item");
    check(game.character_->addItem(3, 0, 0), "add third fixture item");
    game.gameCanvas_->player_ = game.character_;
    check(game.character_->itemCount_ > 2, "fixture has multiple inventory rows");
    if (failures) return 1;
    const auto countBefore = game.character_->itemCount_;
    const auto retainedItem = game.character_->inventory_[2];
    game.InventoryUI_ = game.newInventoryUI();
    game.InventoryUI_->backTarget_ = game.mainMenuUI_;
    game.setCurrentDisplay(game.InventoryUI_);
    current(&game)->setSelectedIndex(1);
    game.commandAction(select, nullptr);
    check(current(&game)->screenId_ == 34 && game.currentItemIndex_ == 1,
          "selecting inventory row opens actions for that slot");
    current(&game)->setSelectedIndex(0); // Drop
    game.commandAction(select, nullptr);
    check(game.character_->itemCount_ == countBefore - 1 && game.character_->inventory_[1] == retainedItem,
          "Drop removes the selected slot and compacts inventory");
    check(current(&game)->screenId_ == 33 && game.currentItemIndex_ == -1,
          "Drop rebuilds inventory and clears pending item");
    check(current(&game)->backTarget_ == game.mainMenuUI_,
          "rebuilt inventory preserves its dynamic root");
#ifdef COMMAND_GAME_DAWNSTAR
    check(current(&game)->selectedIndex() == 1, "Dawnstar restores inventory selection");
#else
    check(current(&game)->selectedIndex() == 0, "Stormhold starts rebuilt inventory at first row");
#endif

    game.character_->knownSpells_ = (1 << 0) | (1 << 2) | (1 << 4);
    game.SpellsListUI_ = game.newSpellsListUI();
    game.SpellsListUI_->backTarget_ = game.mainMenuUI_;
    game.setCurrentDisplay(game.SpellsListUI_);
    current(&game)->setSelectedIndex(1);
    game.commandAction(select, nullptr);
    check(current(&game)->screenId_ == 38 && game.currentSpellIndex_ == 1,
          "spell row opens details and remembers row index");
    game.commandAction(select, nullptr);
    check(game.character_->readiedSpell_ == 3 && game.currentSpellIndex_ == -1,
          "ready spell maps sparse known-spell row to one-based spell ID");
    check(current(&game)->screenId_ == 37, "ready spell returns to rebuilt spell list");
    check(current(&game)->backTarget_ == game.mainMenuUI_,
          "rebuilt spell list preserves its dynamic root");
#ifdef COMMAND_GAME_DAWNSTAR
    check(current(&game)->selectedIndex() == 1, "Dawnstar restores spell selection");
#else
    check(current(&game)->selectedIndex() == 0, "Stormhold starts rebuilt spell list at first row");
#endif

    for (int response : {21, 23, 24, 25, 353, 355}) {
        message->screenId_ = response;
        message->contextIndex_ = 0;
        message->backTarget_ = nullptr;
        game.NPCChoicesUI_[0]->setBodyText("stale aid display");
        game.setCurrentDisplay(message);
        game.commandAction(select, nullptr);
        check(current(&game) == message, "NPC response ignores Select");
        game.commandAction(ok, nullptr);
        check(current(&game) == game.NPCChoicesUI_[0], "NPC response returns to contextual choices");
        check(game.NPCChoicesUI_[0]->bodyText() != "stale aid display",
              "NPC response refreshes aid display before returning");
    }
    message->backTarget_ = game.mainMenuUI_;
    game.NPCChoicesUI_[0]->setBodyText("stale aid display");
    game.setCurrentDisplay(message);
    game.commandAction(cancel, nullptr);
    check(current(&game) == game.mainMenuUI_ && game.NPCChoicesUI_[0]->bodyText() == "stale aid display",
          "global Cancel bypasses NPC response refresh");

    for (bool options : {false, true}) {
        game.setCurrentDisplay(options ? game.OptionsUI_ : game.mainMenuUI_);
        current(&game)->setSelectedIndex(menuaction::rowOf(
            options ? game.profile().optionsRows : menuaction::kMainMenuRows,
            options ? game.profile().optionsRowCount : menuaction::kMainMenuRowCount,
            menuaction::LOAD_GAME));
        jobSeen = screenAtJob = -1;
        game.commandAction(select, nullptr);
        check(jobSeen == 6 && current(&game)->screenId_ == 302, "Load dispatches job and opens progress");
#ifdef COMMAND_GAME_DAWNSTAR
        check(screenAtJob == 302, "Dawnstar shows load progress before dispatching job");
        check(game.noSavedGameUI_->backTarget_ == (options ? game.OptionsUI_ : game.mainMenuUI_),
              "Dawnstar load failure remembers source menu");
#else
        check(screenAtJob == (options ? 31 : 2), "Stormhold dispatches load job before showing progress");
#endif
    }
    game.setCurrentDisplay(game.OptionsUI_);
    current(&game)->setSelectedIndex(menuaction::rowOf(game.profile().optionsRows, game.profile().optionsRowCount, menuaction::SAVE_GAME));
    game.commandAction(select, nullptr);
    check(jobSeen == 5 && current(&game)->screenId_ == 303, "Save dispatches job and opens progress");
#ifdef COMMAND_GAME_DAWNSTAR
    check(screenAtJob == 303, "Dawnstar shows save progress before job");
#else
    check(screenAtJob == 31, "Stormhold dispatches save job before progress");
#endif

    game.character_->levelUpMask_ = 7; // Three offered base attributes.
    game.character_->vitals_[1] = 10;
    const int before[] = {game.character_->attributes_[0], game.character_->attributes_[2], game.character_->attributes_[4]};
    game.LevelUpUI_ = game.newLevelUpUI(1);
    game.setCurrentDisplay(game.LevelUpUI_);
    for (int choice = 0; choice < 3; ++choice) {
        check(current(&game)->screenId_ == 39 && current(&game)->contextIndex_ == choice,
              "level-up advances choice context");
        current(&game)->setSelectedIndex(choice);
        game.commandAction(select, nullptr);
    }
    check(game.character_->attributes_[0] == before[0] + 3 &&
          game.character_->attributes_[2] == before[1] + 2 &&
          game.character_->attributes_[4] == before[2] + 1,
          "level-up applies choices to attributes identified by label");
    check(game.character_->vitals_[1] == 0, "level-up consumes ten XP");
#ifdef COMMAND_GAME_DAWNSTAR
    check(game.character_->levelUpMask_ == 0, "Dawnstar spendLevelUp clears attribute mask");
#else
    check(game.character_->levelUpMask_ == 7, "Stormhold spendLevelUp retains attribute mask");
#endif
    check(current(&game) == nullptr, "level-up returns to gameplay");
    return failures ? 1 : 0;
}
