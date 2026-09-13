#ifndef DAWNSTAR_HOST_HOST_HPP
#define DAWNSTAR_HOST_HOST_HPP

#include <memory>

#include "src/common/host/game_host.hpp"

namespace dawnstar {

std::unique_ptr<host::GameHost> makeHost();

}

#endif
