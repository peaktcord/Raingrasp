#include "src/common/game/menuaction.hpp"

namespace menuaction {

Action at(const Action *rows, int32_t count, int32_t row) {
    if (row < 0 || row >= count) {
        return NONE;
    }
    return rows[row];
}

int32_t rowOf(const Action *rows, int32_t count, Action action) {
    for (int32_t n = 0; n < count; ++n) {
        if (rows[n] == action) {
            return n;
        }
    }
    return -1;
}

const char *optionsLabel(Action action) {
    switch (action) {
        case STATS: return "Stats";
        case INVENTORY: return "Inventory";
        case CLUE_LOG: return "Clue Log";
        case SKILLS: return "Skills";
        case SPELLS: return "Spells";
        case SAVE_GAME: return "Save Game";
        case LOAD_GAME: return "Load Game";
        case HELP: return "Help";
        case REVEAL_TRAITOR: return "Reveal Traitor";
        case PORT_OPTIONS: return "Display & Controls";
        case GAME_SELECT: return "Game Select";
        case QUIT: return "Quit Game";
        default: return "";
    }
}

}
