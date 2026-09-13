#include "src/stormhold/input/shortcut_host.hpp"

#include "src/common/input/game_shortcut_host.hpp"

namespace stormhold_shortcuts {

void install(Game **gameSlot) { game_shortcuts::install(gameSlot); }

const shortcuts::Host *host() { return game_shortcuts::host(); }

}
