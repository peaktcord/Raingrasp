#ifndef COMMON_GAME_SMALLHELPERS_HPP
#define COMMON_GAME_SMALLHELPERS_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

namespace smallhelpers {

enum TileBit {
    WALL = 1,
    MONSTER = 2,
    CHAMPION = 0x20,
};

bool isWalkable(int8_t tile);

SharedArray<std::string> readStringTable(BinaryReader *in);

SharedArray<std::string> readNpcMessages(int32_t npc, int32_t expected, BinaryReader *in);

int32_t saveSize(bool full);

std::string describeSpell(const SharedArray<std::string> &skillNames, int32_t spell);

}

#endif
