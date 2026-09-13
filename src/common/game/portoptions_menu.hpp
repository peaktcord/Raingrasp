#ifndef COMMON_GAME_PORTOPTIONS_MENU_HPP
#define COMMON_GAME_PORTOPTIONS_MENU_HPP

#include "src/common/runtime.hpp"
#include "src/common/platform/platform.hpp"

namespace portoptions {

enum Row {
    WIDESCREEN = 0,
    FULLSCREEN,
    LETTER_KEYS,
    MOVE_AUTOREPEAT,
    HANDSET_DEFAULTS,
    ROW_COUNT
};

SharedArray<std::string> labels(const platform::PortOptions &options);

void select(int32_t row, platform::PortOptions *options);

}

#endif
