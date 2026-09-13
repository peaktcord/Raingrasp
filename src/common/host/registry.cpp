#include "src/common/host/registry.hpp"

#include "src/dawnstar/host/host.hpp"
#include "src/stormhold/host/host.hpp"

namespace host {

std::unique_ptr<GameHost> createHost(const std::string &id) {
    if (id == "dawnstar") return dawnstar::makeHost();
    if (id == "stormhold") return stormhold::makeHost();
    return nullptr;
}

std::vector<std::string> hostIds() { return {"dawnstar", "stormhold"}; }

}
