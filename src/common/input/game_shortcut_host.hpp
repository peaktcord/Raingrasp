#ifndef COMMON_GAME_SHORTCUT_HOST_HPP
#define COMMON_GAME_SHORTCUT_HOST_HPP

#include "src/common/input/menu_shortcuts.hpp"

class Game;

namespace game_shortcuts {

void install(Game **gameSlot, int32_t extraRootScreen = -1);
const shortcuts::Host *host();

}

#endif
