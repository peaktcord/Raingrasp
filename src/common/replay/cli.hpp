#ifndef COMMON_REPLAY_CLI_HPP
#define COMMON_REPLAY_CLI_HPP

#include "src/common/replay/replay.hpp"

namespace replay {

int runCli(Probe &probe, const Script &script, const char *defaultSaveDir,
           int argc, char **argv);

}

#endif
