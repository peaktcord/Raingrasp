#ifndef COMMON_REPLAY_PLAYER_HARNESS_HPP
#define COMMON_REPLAY_PLAYER_HARNESS_HPP

#include "src/common/game/profile.hpp"

namespace player_harness {

int run(int argc, char **argv, const game::Profile &profile);

}

#endif
