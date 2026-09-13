// Display & Controls updates in place, and Select or Left/Right changes a
// two-state row without disturbing selection, scroll, or command bindings.
//
// `commandflow::navigate` reports a command *recognized* as soon as a rule
// matches the screen, whatever the command was. Both profiles carried a
// {SCREEN_PORT_OPTIONS, Any, Back} rule, and navigation runs before the menu
// handler, so every Select was claimed by navigation and routed to Back: the
// screen went back a menu instead of toggling. Narrowing the rule to Back does
// not help -- the claim is on the screen, not the command -- so both rules are
// gone and the menu handler routes Back through the widget's own backTarget_.
//
// Underneath that, re-running setupList on the live widget *appended* Select
// and Back, growing the command list by two per toggle. menupaint resolves the
// soft keys only for a one- or two-command list, so past that both keys came
// unbound. Labels are now replaced directly on the same widget.
//
// This pins both: the values toggle, and the command list stays at two with
// the keys still resolving, however many times a row is selected.
#include <cstdio>
#include "src/common/platform/desktop.hpp"
#include "src/common/replay/headless.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/menuaction.hpp"
#include "src/common/game/portoptions_menu.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#ifdef PORTKEY_GAME_DAWNSTAR
#include "src/dawnstar/dungeon.hpp"
#include "src/dawnstar/variant.hpp"
#include "src/dawnstar/profile.hpp"
using namespace dawnstar;
#else
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/variant.hpp"
#include "src/stormhold/profile.hpp"
using namespace stormhold;
#endif

namespace {
int failures = 0;
void check(bool value, const char *message) {
    std::printf("%s %s\n", value ? "ok  " : "FAIL", message);
    if (!value) ++failures;
}
void noCanvasLoop(GameCanvas *) {}
void noSplashLoop(UIWidget *) {}
}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/port-options-keys-test");
    auto *context = platform::defaultContext();
#ifdef PORTKEY_GAME_DAWNSTAR
    dawnstar_init_statics(context);
#else
    stormhold_init_statics(context);
#endif
    auto *select = UIWidget::cmdSelect_;
    Game game(profile(), context);
    game.setExecutionHooks(headless::runJobInline<Game>, noCanvasLoop, noSplashLoop);
    game.startApplication();

    const int32_t settingsRow = menuaction::rowOf(
        game.profile().optionsRows, game.profile().optionsRowCount, menuaction::PORT_OPTIONS);
#ifdef PORTKEY_GAME_DAWNSTAR
    check(settingsRow == 5, "Display & Controls precedes Save/Load in Dawnstar");
#else
    check(settingsRow == 4, "Display & Controls precedes Save/Load in Stormhold");
#endif
    check(game.OptionsUI_->rowLabels()[settingsRow] == "Display & Controls",
          "the parent Options row uses the player-facing name");
    const int32_t mainSettingsRow = menuaction::rowOf(
        menuaction::kMainMenuRows, menuaction::kMainMenuRowCount, menuaction::PORT_OPTIONS);
    check(mainSettingsRow == 2, "Display & Controls is available near the top of the main menu");
    check(game.mainMenuUI_->rowLabels()[mainSettingsRow] == "Display & Controls",
          "the main-menu row uses the player-facing name");

    // Open it from the Options list, as a player does: performMenuAction takes
    // the *current* screen as the back target, so the list has to be up first
    // or Back has nowhere real to return to.
    game.setCurrentDisplay(game.OptionsUI_);
    game.performMenuAction(menuaction::PORT_OPTIONS, true);
    UIWidget *list = game.currentUI_;
    check(list != nullptr && list->screenId_ == uistate::SCREEN_PORT_OPTIONS,
          "Display & Controls is the current screen");
    if (list == nullptr) return 1;

    check(list->commands_.size() == 2, "a fresh list carries exactly Select and Back");
    check(list->positiveCommand() == select, "the select key resolves to Select");
    check(list->negativeCommand() == UIWidget::cmdBack_, "the back key resolves to Back");

    check(list->rowLabels()[portoptions::WIDESCREEN] == "View: Classic",
          "view row describes the rendered mode");
    check(list->rowLabels()[portoptions::FULLSCREEN] == "Window: Windowed",
          "window row describes the window mode");
    check(list->rowLabels()[portoptions::LETTER_KEYS] == "HUD keys: Letters",
          "HUD row describes which labels are shown");
    check(list->rowLabels()[portoptions::MOVE_AUTOREPEAT] == "Movement: Tap",
          "movement row describes the input behavior");
    check(list->rowLabels()[portoptions::HANDSET_DEFAULTS] == "Handset defaults",
          "preset row names the state it applies");
    for (int32_t row = 0; row < list->rowCount(); ++row) {
        check(UIWidget::bodyFont_->stringWidth(list->rowLabels()[row]) <= 146,
              "a settings label fits inside the list margins");
    }

    // Toggle the same row several times. Each pass refreshes labels in place;
    // replacing the entire widget here used to make the interaction flicker.
    const bool first = context->portOptions().moveAutorepeat;
    for (int pass = 1; pass <= 4; ++pass) {
        game.currentUI_->setSelectedIndex(portoptions::MOVE_AUTOREPEAT);
        game.commandAction(select, nullptr);

        UIWidget *now = game.currentUI_;
        char message[96];

        std::snprintf(message, sizeof message,
                      "toggle %d: still on Display & Controls", pass);
        check(now == list && now->screenId_ == uistate::SCREEN_PORT_OPTIONS, message);
        if (now == nullptr) return 1;

        std::snprintf(message, sizeof message,
                      "toggle %d: the command list is still two commands", pass);
        check(now->commands_.size() == 2, message);

        std::snprintf(message, sizeof message,
                      "toggle %d: the select key still resolves to Select", pass);
        check(now->positiveCommand() == select, message);

        std::snprintf(message, sizeof message,
                      "toggle %d: the back key still resolves to Back", pass);
        check(now->negativeCommand() == UIWidget::cmdBack_, message);

        // And the toggle actually happened: odd passes flip, even ones restore.
        const bool expected = (pass % 2 == 1) ? !first : first;
        std::snprintf(message, sizeof message,
                      "toggle %d: hold-to-move is %s", pass, expected ? "on" : "off");
        check(context->portOptions().moveAutorepeat == expected, message);

        std::snprintf(message, sizeof message,
                      "toggle %d: the highlight stayed on the row", pass);
        check(now->selectedIndex() == portoptions::MOVE_AUTOREPEAT, message);
    }

    // The key path itself: -7 is what Enter arrives as, and it must reach the
    // handler rather than falling through to list navigation.
    const bool beforeKey = context->portOptions().moveAutorepeat;
    game.currentUI_->setSelectedIndex(portoptions::MOVE_AUTOREPEAT);
    game.currentUI_->keyPressed(-7);
    check(context->portOptions().moveAutorepeat != beforeKey,
          "the select key (-7) toggles rather than navigating away");
    check(game.currentUI_ != nullptr &&
              game.currentUI_->screenId_ == uistate::SCREEN_PORT_OPTIONS,
          "and leaves the player on Display & Controls");

    // Left and Right cycle a value through the same command path. They do not
    // navigate away or act on the one-way Handset defaults row.
    const bool beforeLeft = context->portOptions().moveAutorepeat;
    game.currentUI_->keyPressed(-3);
    check(context->portOptions().moveAutorepeat != beforeLeft,
          "Left changes the selected setting");
    game.currentUI_->keyPressed(-4);
    check(context->portOptions().moveAutorepeat == beforeLeft,
          "Right changes the selected setting");
    game.currentUI_->setSelectedIndex(portoptions::HANDSET_DEFAULTS);
    context->portOptions().letterKeyLabels = true;
    game.currentUI_->keyPressed(-3);
    check(context->portOptions().letterKeyLabels,
          "Left does not activate the Handset defaults action");

    // A host shortcut such as Alt+Enter changes the shared state outside the
    // widget. Refreshing must update an already-open settings screen.
    context->portOptions().fullscreen = true;
    game.refreshPortOptionsUI();
    check(game.currentUI_->rowLabels()[portoptions::FULLSCREEN] == "Window: Fullscreen",
          "an out-of-band fullscreen change refreshes the open row");

    // Back is the other half of the routing: with no navigation rule for this
    // screen, leaving relies on the widget's own backTarget_.
    game.commandAction(UIWidget::cmdBack_, nullptr);
    check(game.currentUI_ != nullptr &&
              game.currentUI_->screenId_ == uistate::SCREEN_OPTIONS,
          "Back leaves Display & Controls for the Options list");

    // The same screen is reachable before starting or continuing a game, and
    // Back returns to the menu it was opened from.
    game.setCurrentDisplay(game.mainMenuUI_);
    game.currentUI_->setSelectedIndex(mainSettingsRow);
    game.commandAction(select, nullptr);
    check(game.currentUI_ != nullptr &&
              game.currentUI_->screenId_ == uistate::SCREEN_PORT_OPTIONS,
          "main-menu Display & Controls opens the settings screen");
    game.commandAction(UIWidget::cmdBack_, nullptr);
    check(game.currentUI_ == game.mainMenuUI_,
          "Back returns Display & Controls to the main menu");

    std::printf(failures ? "FAILED\n" : "PASSED\n");
    return failures ? 1 : 0;
}
