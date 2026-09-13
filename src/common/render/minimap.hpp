#ifndef COMMON_RENDER_MINIMAP_HPP
#define COMMON_RENDER_MINIMAP_HPP

#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

namespace minimap {

struct Palette {
    int32_t wall = 0x000000;
    int32_t floor = 0xFFFFFF;
    int32_t bit2 = 0xFF0000;
    int32_t bit4 = 0x0000FF;
    int32_t portal = 0xCC00FF;
    int32_t player = 0x00FF00;
};

void plot(Graphics *graphics, const SharedArray<SharedArray<int8_t>> &cells, int32_t originX,
          int32_t originY, int32_t cols, int32_t cellSize,
          const Palette &palette = Palette());

}

#endif
