#include "src/common/game/menupaint.hpp"
#include "src/common/game/uistate.hpp"

namespace menupaint {
namespace {

void drawArrow(Graphics *graphics, int32_t x, int32_t y, int32_t direction) {
    (void)graphics->getColor();
    graphics->setColor(0);
    int32_t height = 5;
    if (direction == 1) {
        int32_t row = 0;
        while (row < height) {
            graphics->drawLine(x - row, y + row, x + row, y + row);
            ++row;
        }
    } else {
        int32_t row = 0;
        while (row < height) {
            graphics->drawLine(x - (height - row), y + row, x + (height - row), y + row);
            ++row;
        }
    }
}

}

void drawTitleBar(Graphics *graphics, int32_t width, Font *font, const std::string &title) {
    Font *previousFont = graphics->getFont();
    graphics->setFont(font);
    int32_t previousColor = graphics->getColor();
    graphics->setColor(0);
    graphics->fillRect(0, 0, width, 14);
    graphics->setColor(0xFFFFFF);
    graphics->drawString(title, width / 2, 0, 17);
    graphics->setColor(previousColor);
    graphics->setFont(previousFont);
}

void drawScrollArrows(Graphics *graphics, int32_t first, int32_t last, int32_t count) {
    if (first > 0) {
        drawArrow(graphics, 155, 180, 1);
    }
    if (last + 1 < count) {
        drawArrow(graphics, 165, 180, 2);
    }
}

Command *positiveCommand(const std::vector<Command *> &commands, Command *ok, Command *select) {
    int32_t size = (int32_t)commands.size();
    if (size == 1) {
        return commands[0];
    }
    if (size != 2) {
        return nullptr;
    }
    int32_t n = 0;
    while (n < 2) {
        Command *command = commands[(size_t)n];
        if (command == ok || command == select) {
            return command;
        }
        ++n;
    }
    return nullptr;
}

Command *negativeCommand(const std::vector<Command *> &commands, Command *back, Command *cancel) {
    if (commands.size() != 2) {
        return nullptr;
    }
    int32_t n = 0;
    while (n < 2) {
        Command *command = commands[(size_t)n];
        if (command == back || command == cancel) {
            return command;
        }
        ++n;
    }
    return nullptr;
}

void drawSoftKeys(Graphics *graphics, int32_t width, Font *font,
                  const std::vector<Command *> &commands,
                  Command *negative, Command *positive, int32_t negativeY, int32_t positiveY) {
    if (commands.empty()) {
        return;
    }
    graphics->setColor(0xFFFFFF);
    graphics->fillRect(0, 190, width, 20);
    graphics->setColor(0);
    graphics->setFont(font);
    if (negative != nullptr) {
        graphics->drawString(negative->getLabel(), 10, negativeY, 20);
    }
    if (positive != nullptr) {
        graphics->drawString(positive->getLabel(), width - 10, positiveY, 24);
    }
}

void drawProgressDialog(Graphics *graphics, int32_t width, int32_t height, Font *font, int32_t stateId,
                        int32_t progressPct, int32_t bgColor) {
    graphics->setColor(bgColor);
    graphics->fillRect(0, 0, width, 20 + height);
    graphics->setFont(font);
    graphics->setColor(0xFFFFFF);
    int32_t n = width / 2;
    if (stateId == uistate::LAYOUT_PROGRESS_NEW_GAME) {
        graphics->drawString("Creating New Game", n, 30, 17);
    } else if (stateId == uistate::LAYOUT_PROGRESS_LOAD_GAME) {
        graphics->drawString("Loading Game", n, 30, 17);
    } else if (stateId == uistate::LAYOUT_PROGRESS_SAVE_GAME) {
        graphics->drawString("Saving Game", n, 30, 17);
    } else if (stateId == uistate::LAYOUT_PROGRESS_LOAD_DUNGEON) {
        graphics->drawString("Loading Dungeon", n, 30, 17);
    }
    graphics->drawString("Please Wait", n, 45, 17);
    graphics->setColor(0xFFFFFF);
    graphics->fillRect((width - 90) / 2, 60, 90, 20);
    int32_t n2 = progressPct * 88 / 100;
    graphics->setColor(255);
    graphics->fillRect((width - 88) / 2, 61, n2, 18);
}

void drawRows(Graphics *graphics, const SharedArray<std::string> &rows, int32_t first, int32_t last,
              int32_t selectedLine, int32_t selectedSpan, int32_t marginLeft, int32_t width,
              int32_t lineHeight, int32_t *drawY) {
    int32_t line = first;
    while (line <= last) {
        if (line == selectedLine) {
            graphics->setColor(0x666666);
            graphics->fillRect(marginLeft - 10, *drawY - 1, width - 2 * (marginLeft - 10),
                               selectedSpan * lineHeight + 2);
        }
        graphics->setColor(0xFFFF00);
        graphics->drawString(rows[line], marginLeft, *drawY, 20);
        *drawY += lineHeight;
        ++*drawY;
        ++line;
    }
}

}
