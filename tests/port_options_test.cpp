// The Display & Controls defaults and its handset preset.
//
// These are deliberate choices rather than incidental ones, and the one that
// matters most is `hold-to-move`: the original had no movement autorepeat at
// all (see docs/DIVERGENCES.md), so it is the single row that starts off,
// while every other default is this build's existing behaviour.
#include <cstdio>
#include "src/common/platform/platform.hpp"

namespace {
int failures = 0;
void check(bool value, const char *message) {
    std::printf("%s %s\n", value ? "ok  " : "FAIL", message);
    if (!value) ++failures;
}

}

int main() {
    // Defaults, straight out of the struct.
    platform::PortOptions options;
    check(!options.moveAutorepeat, "hold-to-move defaults off (the original had no repeat)");
    check(options.letterKeyLabels, "letter key labels default on");
    check(!options.widescreen, "widescreen defaults off");
    check(!options.fullscreen, "fullscreen defaults off");
    // The menu preset restores handset presentation and input wholesale.
    options.widescreen = true;
    options.fullscreen = true;
    options.moveAutorepeat = true;
    options.useHandsetDefaults();
    check(!options.moveAutorepeat && !options.letterKeyLabels && !options.widescreen,
          "handset defaults disable port presentation and movement additions");

    // The preset leaves fullscreen alone: it is a property of the window.
    check(options.fullscreen, "handset defaults preserve the window mode");

    std::printf(failures ? "FAILED\n" : "PASSED\n");
    return failures ? 1 : 0;
}
