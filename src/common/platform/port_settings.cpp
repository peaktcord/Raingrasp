#include "src/common/platform/port_settings.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace platform {

namespace {

const char *kFileName = "portoptions.txt";

std::string filePath(const std::string &settingsDir) {
    return settingsDir + "/" + kFileName;
}

void apply(PortOptions *options, const std::string &key, bool value) {
    if (key == "widescreen") options->widescreen = value;
    else if (key == "fullscreen") options->fullscreen = value;
    else if (key == "letterKeyLabels") options->letterKeyLabels = value;
    else if (key == "moveAutorepeat") options->moveAutorepeat = value;
}

}

void loadPortOptions(const std::string &settingsDir, PortOptions *options) {
    if (options == nullptr) return;
    std::FILE *f = std::fopen(filePath(settingsDir).c_str(), "rb");
    if (f == nullptr) return;

    char line[128];
    while (std::fgets(line, (int)sizeof(line), f) != nullptr) {
        char *eq = std::strchr(line, '=');
        if (eq == nullptr) continue;
        *eq = '\0';
        const std::string key(line);
        const char value = eq[1];
        if (value == '0' || value == '1') apply(options, key, value == '1');
    }
    std::fclose(f);
}

void savePortOptions(const std::string &settingsDir, const PortOptions &options) {
    std::error_code ec;
    fs::create_directories(settingsDir, ec);
    std::FILE *f = std::fopen(filePath(settingsDir).c_str(), "wb");
    if (f == nullptr) return;
    std::fprintf(f, "widescreen=%d\n", options.widescreen ? 1 : 0);
    std::fprintf(f, "fullscreen=%d\n", options.fullscreen ? 1 : 0);
    std::fprintf(f, "letterKeyLabels=%d\n", options.letterKeyLabels ? 1 : 0);
    std::fprintf(f, "moveAutorepeat=%d\n", options.moveAutorepeat ? 1 : 0);
    std::fclose(f);
}

}
