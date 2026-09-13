// The SDL loop draws a Form overlay itself, because MIDP would have drawn a
// native screen and nothing here can. That overlay must reflect what is
// current *after* the frame's events and tick have run, not before: pressing
// Enter on the name form fires its Ok, and the game moves on to the Welcome
// box within the same iteration. Drawing the pre-event value paints the
// dismissed form over the screen that replaced it, so it has to be dismissed
// a second time -- which is what a player sees as an extra confirm.
//
// main_sdl.cpp is a binary with an SDL window, so this pins the ordering
// against the same host seam the loop drives rather than against the loop.
#include <cstdio>
#include "src/common/platform/desktop.hpp"
#include "src/common/replay/headless.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/ui.hpp"
#ifdef OVERLAY_GAME_DAWNSTAR
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

Form *currentForm(Display *display) {
    return dynamic_cast<Form *>(display->getCurrent());
}
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/form-overlay-test");
    auto *context = platform::defaultContext();
#ifdef OVERLAY_GAME_DAWNSTAR
    dawnstar_init_statics(context);
#else
    stormhold_init_statics(context);
#endif
    auto *ok = UIWidget::cmdOk_;
    auto *select = UIWidget::cmdSelect_;
    Game game(profile(), context);
    game.setExecutionHooks(headless::runJobInline<Game>, noCanvasLoop, noSplashLoop);
    game.startApplication();

    // Walk to the name form the way the front end does.
    game.setCurrentDisplay(game.newGameUI_);
    game.commandAction(select, nullptr);            // pick a class
    if (game.currentUI_ != nullptr &&
        game.currentUI_->screenId_ == uistate::SCREEN_CHARACTER_SHEET) {
        game.currentUI_->setSelectedIndex(1);       // "Create Character"
        game.commandAction(select, nullptr);
    }
    if (game.currentUI_ != nullptr &&
        game.currentUI_->screenId_ == uistate::SCREEN_CHARACTER_CREATED) {
        game.commandAction(game.profile().characterCreatedSelects ? select : ok,
                           nullptr);
    }

    Display *display = game.display_;
    Form *before = currentForm(display);
    check(before != nullptr, "the name form is current before Enter");
    if (before == nullptr) return 1;

    // Exactly what the loop's Enter handling does.
    TextField *field = (TextField *)before->get(1);
    field->setString(std::string("Tester"));
    const std::vector<Command *> &commands = before->commands();
    check(!commands.empty() && commands[0] == ok, "Enter fires the form's Ok");
    before->listener()->commandAction(commands[0], before);

    // The overlay is drawn from a re-read of what is current. After the Ok the
    // form is gone, so a re-read must not still hand back a form to paint.
    Form *after = currentForm(display);
    check(after == nullptr,
          "no form is current after the name is accepted (re-read, not stale)");
    check(game.currentUI_ != nullptr &&
              game.currentUI_->screenId_ == uistate::SCREEN_WELCOME,
          "the Welcome box is what the player should now see");

    std::printf(failures ? "FAILED\n" : "PASSED\n");
    return failures ? 1 : 0;
}
