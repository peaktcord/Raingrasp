#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "src/common/game/menupaint.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/ui.hpp"
#include "src/common/render/render.hpp"

namespace {

// Legacy Dawnstar drawing logic
void drawLegacyDawnstar(Graphics *graphics, int32_t width, int32_t height, Font *font, int32_t stateId, int32_t progressPct) {
    graphics->setColor(2510210);
    graphics->fillRect(0, 0, width, 20 + height);
    graphics->setFont(font);
    graphics->setColor(0xFFFFFF);
    int32_t n = width / 2;
    if (stateId == 8) {
        graphics->drawString("Creating New Game", n, 30, 17);
    } else if (stateId == 9) {
        graphics->drawString("Loading Game", n, 30, 17);
    } else if (stateId == 10) {
        graphics->drawString("Saving Game", n, 30, 17);
    } else if (stateId == 11) {
        graphics->drawString("Loading Dungeon", n, 30, 17);
    }
    graphics->drawString("Please Wait", n, 45, 17);
    graphics->setColor(0xFFFFFF);
    graphics->fillRect((width - 90) / 2, 60, 90, 20);
    int32_t n2 = progressPct * 88 / 100;
    graphics->setColor(255);
    graphics->fillRect((width - 88) / 2, 61, n2, 18);
}

// Legacy Stormhold drawing logic
void drawLegacyStormhold(Graphics *graphics, int32_t width, int32_t height, Font *font, int32_t stateId, int32_t progressPct) {
    graphics->setColor(11429934);
    graphics->fillRect(0, 0, width, 20 + height);
    graphics->setFont(font);
    graphics->setColor(0xFFFFFF);
    int32_t n = width / 2;
    if (stateId == 8) {
        graphics->drawString(std::string("Creating New Game"), n, 30, 17);
    } else if (stateId == 9) {
        graphics->drawString(std::string("Loading Game"), n, 30, 17);
    } else if (stateId == 10) {
        graphics->drawString(std::string("Saving Game"), n, 30, 17);
    } else if (stateId == 11) {
        graphics->drawString(std::string("Loading Dungeon"), n, 30, 17);
    }
    graphics->drawString(std::string("Please Wait"), n, 45, 17);
    graphics->setColor(0xFFFFFF);
    graphics->fillRect((width - 90) / 2, 60, 90, 20);
    int32_t n2 = progressPct * 88 / 100;
    graphics->setColor(255);
    graphics->fillRect((width - 88) / 2, 61, n2, 18);
}

const char *stateName(int stateId) {
    switch (stateId) {
        case uistate::LAYOUT_PROGRESS_NEW_GAME: return "new_game";
        case uistate::LAYOUT_PROGRESS_LOAD_GAME: return "loading_game";
        case uistate::LAYOUT_PROGRESS_SAVE_GAME: return "saving_game";
        case uistate::LAYOUT_PROGRESS_LOAD_DUNGEON: return "loading_dungeon";
        default: return "unknown";
    }
}

}  // namespace

int main() {
    std::filesystem::create_directories("verify");
    Font *font = Font::getFont(0, 1, 0); // System Bold Medium (used by both games)
    const int32_t width = 176;
    const int32_t height = 208;

    const int states[] = {
        uistate::LAYOUT_PROGRESS_NEW_GAME,
        uistate::LAYOUT_PROGRESS_LOAD_GAME,
        uistate::LAYOUT_PROGRESS_SAVE_GAME,
        uistate::LAYOUT_PROGRESS_LOAD_DUNGEON,
    };
    const int progressValues[] = {0, 25, 50, 75, 100};

    int testsPassed = 0;
    int capturesWritten = 0;

    for (int stateId : states) {
        for (int pct : progressValues) {
            // Test 1: Dawnstar theme (bgColor = 2510210)
            {
                render::Surface legacySurface(width, height);
                render::SoftGraphics legacyGfx(&legacySurface);
                drawLegacyDawnstar(&legacyGfx, width, height, font, stateId, pct);

                render::Surface unifiedSurface(width, height);
                render::SoftGraphics unifiedGfx(&unifiedSurface);
                menupaint::drawProgressDialog(&unifiedGfx, width, height, font, stateId, pct, 2510210);

                assert(legacySurface.pixels == unifiedSurface.pixels && "Dawnstar progress dialog mismatch!");
                ++testsPassed;

                char filename[256];
                std::snprintf(filename, sizeof(filename),
                              "verify/dawnstar_%s_%dpct.png",
                              stateName(stateId), pct);
                bool ok = render::writePng(unifiedSurface, filename);
                assert(ok);
                ++capturesWritten;
            }

            // Test 2: Stormhold theme (bgColor = 11429934)
            {
                render::Surface legacySurface(width, height);
                render::SoftGraphics legacyGfx(&legacySurface);
                drawLegacyStormhold(&legacyGfx, width, height, font, stateId, pct);

                render::Surface unifiedSurface(width, height);
                render::SoftGraphics unifiedGfx(&unifiedSurface);
                menupaint::drawProgressDialog(&unifiedGfx, width, height, font, stateId, pct, 11429934);

                assert(legacySurface.pixels == unifiedSurface.pixels && "Stormhold progress dialog mismatch!");
                ++testsPassed;

                char filename[256];
                std::snprintf(filename, sizeof(filename),
                              "verify/stormhold_%s_%dpct.png",
                              stateName(stateId), pct);
                bool ok = render::writePng(unifiedSurface, filename);
                assert(ok);
                ++capturesWritten;
            }
        }
    }

    std::printf("PASS: %d progress dialog parity tests passed, %d screenshots written to verify/\n",
                testsPassed, capturesWritten);
    return 0;
}
