#ifndef COMMON_GAME_MENUACTION_HPP
#define COMMON_GAME_MENUACTION_HPP

#include "src/common/runtime.hpp"

namespace menuaction {

enum Action {
    NONE = 0,
    NEW_GAME,
    LOAD_GAME,
    HELP,
    CREDITS,
    GAME_SELECT,
    QUIT,
    STATS,
    INVENTORY,
    CLUE_LOG,
    SKILLS,
    SPELLS,
    SAVE_GAME,
    REVEAL_TRAITOR,
    DEBUG_FORM,
    PORT_OPTIONS
};

inline constexpr Action kMainMenuRows[] = {NEW_GAME, LOAD_GAME, PORT_OPTIONS, HELP,
                                           CREDITS, GAME_SELECT, QUIT};
inline constexpr int32_t kMainMenuRowCount = sizeof(kMainMenuRows) / sizeof(kMainMenuRows[0]);

Action at(const Action *rows, int32_t count, int32_t row);

const char *optionsLabel(Action action);

int32_t rowOf(const Action *rows, int32_t count, Action action);

}

#endif
