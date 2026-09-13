#ifndef PLATFORM_PORT_SETTINGS_HPP
#define PLATFORM_PORT_SETTINGS_HPP

#include <string>

#include "src/common/platform/platform.hpp"

namespace platform {

void loadPortOptions(const std::string &settingsDir, PortOptions *options);
void savePortOptions(const std::string &settingsDir, const PortOptions &options);

}

#endif
