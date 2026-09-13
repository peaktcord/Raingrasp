#ifndef STORMHOLD_SHORTCUT_HOST_HPP
#define STORMHOLD_SHORTCUT_HOST_HPP

#include "src/common/input/menu_shortcuts.hpp"

class Game;
namespace stormhold_shortcuts {

void install(Game **gameSlot);

const shortcuts::Host *host();

}

#endif
