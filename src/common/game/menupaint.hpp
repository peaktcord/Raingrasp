#ifndef COMMON_GAME_MENUPAINT_HPP
#define COMMON_GAME_MENUPAINT_HPP

#include <vector>

#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

namespace menupaint {

void drawTitleBar(Graphics *graphics, int32_t width, Font *font, const std::string &title);

void drawScrollArrows(Graphics *graphics, int32_t first, int32_t last, int32_t count);

Command *positiveCommand(const std::vector<Command *> &commands, Command *ok, Command *select);
Command *negativeCommand(const std::vector<Command *> &commands, Command *back, Command *cancel);

void drawSoftKeys(Graphics *graphics, int32_t width, Font *font,
                  const std::vector<Command *> &commands,
                  Command *negative, Command *positive, int32_t negativeY, int32_t positiveY);

void drawProgressDialog(Graphics *graphics, int32_t width, int32_t height, Font *font, int32_t stateId,
                        int32_t progressPct, int32_t bgColor);

void drawRows(Graphics *graphics, const SharedArray<std::string> &rows, int32_t first, int32_t last,
              int32_t selectedLine, int32_t selectedSpan, int32_t marginLeft, int32_t width,
              int32_t lineHeight, int32_t *drawY);

}

#endif
