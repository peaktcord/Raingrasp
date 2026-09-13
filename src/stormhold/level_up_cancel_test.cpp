// A level-up that arrives while a menu is open used to strip that menu's
// Cancel instead of the level-up screen's own.
//
// newLevelUpUI removed the command through `canvas_`, and WidgetCanvas
// forwards to whatever widget is currently mounted.  The new screen is not
// mounted until setCurrentDisplay runs, which happens after the constructor
// returns -- so the removal landed on the screen the player was already
// looking at.  For the NPC choices screens that is fatal: they are allocated
// once and reused for the rest of the session, and Cancel is their only exit.
// Train with an NPC, level up on the tick that follows, and the menu can
// never be left again.
//
// The test drives the seam directly rather than through a playthrough so it
// needs no ROM data: mount a form, build a level-up screen behind it, and
// check which of the two lost its Cancel.

#include "src/common/game/game.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/stormhold/profile.hpp"
#include "src/stormhold/variant.hpp"

#include <iostream>
#include <memory>

namespace {

int failures = 0;

void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool hasCancel(UIWidget *screen) {
    for (Command *command : screen->commands_) {
        if (command == UIWidget::cmdCancel_) return true;
    }
    return false;
}

}

int main() {
    platform::PlatformContext context;
    stormhold::stormhold_init_statics(&context);

    Game game(stormhold::profile(), &context);

    // newLevelUpUI asks the character which attributes are on offer.  Claim
    // the ROM tables are already loaded so the character can be built without
    // them: an untouched level-up mask offers no attributes, and the rows are
    // not what is under test here.
    Player::dataLoaded_ = true;
    game.characterStorage_ = std::make_unique<Player>(&game, stormhold::profile());
    game.character_ = game.characterStorage_.get();

    // Stand in for an NPC choices screen: a form, mounted the way
    // setCurrentDisplay mounts one.  setupForm gives it Select and Cancel.
    UIWidget *choices = game.makeOwnedUIWidget(5, stormhold::screens::NPC_CHOICES_0);
    SharedArray<std::string> rows{std::string("Train"), std::string("Give")};
    choices->setupForm(std::string("Champion"), std::string("Aid: 1"), rows);
    check(hasCancel(choices), "a freshly built choices screen offers Cancel");

    game.currentUI_ = choices;
    game.uiCanvas_->widget_ = choices;

    // The level-up the training earned, arriving on the next world tick while
    // the choices screen is still the mounted widget.
    UIWidget *levelUp = game.newLevelUpUI(1);

    check(hasCancel(choices),
          "the open NPC menu keeps its Cancel when a level-up screen is built behind it");
    check(!hasCancel(levelUp),
          "the level-up screen itself has no Cancel -- all three points must be spent");

    if (failures == 0) {
        std::cout << "ok: a level-up behind an open menu leaves that menu escapable\n";
    }
    return failures == 0 ? 0 : 1;
}
