#ifndef COMMON_MAP_ATLAS_HPP
#define COMMON_MAP_ATLAS_HPP

#include <string>
#include <vector>

#include "src/common/render/render.hpp"

namespace mapatlas {

enum MarkerKind { MONSTER, CHEST, ITEM };

struct Marker {
    int32_t x = 0;
    int32_t y = 0;
    MarkerKind kind = MONSTER;
};

enum Edge { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 };

struct Dungeon {
    int32_t id = 0;
    int32_t width = 0;
    int32_t height = 0;
    std::string name;
    std::vector<int8_t> cells;
    std::vector<Marker> markers;
    int32_t links[4] = {0, 0, 0, 0};

    int8_t at(int32_t x, int32_t y) const { return cells[(size_t)(x * height + y)]; }
};

bool writeMap(const Dungeon &dungeon, const std::string &path, int32_t cell);

bool writeAtlas(const std::vector<Dungeon> &dungeons, const std::string &path, int32_t cell,
                std::vector<std::string> *conflicts);

}

#endif
