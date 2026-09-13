#include "src/dawnstar/input/shortcut_host.hpp"

#include "src/common/input/game_shortcut_host.hpp"
#include "src/dawnstar/variant.hpp"

namespace dawnstar_shortcuts {

void install(Game **gameSlot) { game_shortcuts::install(gameSlot, dawnstar::screens::CLUE_LIST); }

const shortcuts::Host *host() { return game_shortcuts::host(); }

}
