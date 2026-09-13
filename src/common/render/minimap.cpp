#include "src/common/render/minimap.hpp"

namespace minimap {

void plot(Graphics *graphics, const SharedArray<SharedArray<int8_t>> &cells, int32_t originX,
          int32_t originY, int32_t cols, int32_t cellSize, const Palette &palette) {
    int32_t centre = cols / 2;
    for (int32_t row = 0; row < cols; ++row) {
        int32_t y = originY + row * cellSize;
        for (int32_t column = 0; column < cols; ++column) {
            int32_t x = originX + column * cellSize;
            int8_t cell = cells[column][row];
            if (cell == 1) {
                graphics->setColor(palette.wall);
                graphics->fillRect(x, y, cellSize, cellSize);
            } else if (cell == 0) {
                graphics->setColor(palette.floor);
                graphics->fillRect(x, y, cellSize, cellSize);
            } else if ((cell & 2) != 0) {
                graphics->setColor(palette.bit2);
                graphics->fillRect(x, y, cellSize, cellSize);
            } else if ((cell & 4) != 0) {
                graphics->setColor(palette.bit4);
                graphics->fillRect(x, y, cellSize, cellSize);
            } else if ((cell & 8) != 0) {
                graphics->setColor(palette.portal);
                graphics->fillRect(x, y, cellSize, cellSize);
            }
            if (row == centre && column == centre) {
                graphics->setColor(palette.player);
                graphics->fillRect(x, y, cellSize, cellSize);
            }
        }
    }
}

}
