#ifndef COMMON_MENU_SHORTCUTS_HPP
#define COMMON_MENU_SHORTCUTS_HPP

#include "src/common/runtime.hpp"

namespace shortcuts {

// Port-only list jumps.  The handset had no Home/End/PageUp/PageDown, so these
// are not MIDP game actions and deliberately do not extend
// Canvas::getGameAction, which mirrors the original key table.  They travel as
// key codes through the ordinary keyPressed path and are acted on by the menu
// widget; the game canvas ignores them.
constexpr int32_t JUMP_HOME = 1001;
constexpr int32_t JUMP_END = 1002;
constexpr int32_t JUMP_PAGE_UP = 1003;
constexpr int32_t JUMP_PAGE_DOWN = 1004;

inline bool isJumpKey(int32_t code) {
    return code == JUMP_HOME || code == JUMP_END || code == JUMP_PAGE_UP ||
           code == JUMP_PAGE_DOWN;
}

struct Host {
    void (*pressKey)(int32_t code) = nullptr;
    bool (*optionsOpen)() = nullptr;
    // Selects the requested Options row and returns true when the resulting
    // display is an interactive menu that can be treated as a shortcut root.
    bool (*selectRow)(int32_t row) = nullptr;
    bool (*inGame)() = nullptr;
    bool (*atShortcutRoot)() = nullptr;
    bool (*closeShortcutMenu)() = nullptr;
};

struct Binding {
    int32_t key = 0;
    int32_t row = 0;
    const char *label = nullptr;
};

void install(const Host *host, const Binding *bindings, int count);

bool onKey(int32_t key);

// Consumes Back only at the root opened by a shortcut. Descendant menus keep
// their ordinary one-level Back behavior.
bool onBack();

void tick();

bool pending();

void cancel();

}

#endif
