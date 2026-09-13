#include "src/common/game/portoptions_menu.hpp"

namespace portoptions {

SharedArray<std::string> labels(const platform::PortOptions &options) {
    SharedArray<std::string> out(ROW_COUNT);
    out[WIDESCREEN] = options.widescreen ? "View: Wide 3D" : "View: Classic";
    out[FULLSCREEN] = options.fullscreen ? "Window: Fullscreen" : "Window: Windowed";
    out[LETTER_KEYS] = options.letterKeyLabels ? "HUD keys: Letters" : "HUD keys: Numbers";
    out[MOVE_AUTOREPEAT] = options.moveAutorepeat ? "Movement: Hold" : "Movement: Tap";
    out[HANDSET_DEFAULTS] = std::string("Handset defaults");
    return out;
}

void select(int32_t row, platform::PortOptions *options) {
    if (options == nullptr) return;
    switch (row) {
        case WIDESCREEN: options->widescreen = !options->widescreen; break;
        case FULLSCREEN: options->fullscreen = !options->fullscreen; break;
        case LETTER_KEYS: options->letterKeyLabels = !options->letterKeyLabels; break;
        case MOVE_AUTOREPEAT: options->moveAutorepeat = !options->moveAutorepeat; break;
        case HANDSET_DEFAULTS: options->useHandsetDefaults(); break;
        default: break;
    }
}

}
