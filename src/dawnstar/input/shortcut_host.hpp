#ifndef DAWNSTAR_SHORTCUT_HOST_HPP
#define DAWNSTAR_SHORTCUT_HOST_HPP

#include "src/common/input/menu_shortcuts.hpp"

class Game;
namespace dawnstar_shortcuts {

void install(Game **gameSlot);

const shortcuts::Host *host();

}

#endif
