#ifndef COMMON_HOST_REGISTRY_HPP
#define COMMON_HOST_REGISTRY_HPP

#include <memory>
#include <string>
#include <vector>

#include "src/common/host/game_host.hpp"

namespace host {

std::unique_ptr<GameHost> createHost(const std::string &id);

std::vector<std::string> hostIds();

}

#endif
