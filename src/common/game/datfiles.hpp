#ifndef COMMON_DATFILES_HPP
#define COMMON_DATFILES_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

namespace datfiles {

inline void loadMonsters(BinaryReader *in, int32_t *count, SharedArray<std::string> *names,
                         SharedArray<SharedArray<int8_t>> *stats) {
    *count = in->readInt();
    *names = SharedArray<std::string>(*count);
    *stats = makeSharedArray2D<int8_t>(*count, 17);
    for (int32_t n = 0; n < *count; ++n) (*names)[n] = in->readUTF();
    for (int32_t n = 0; n < *count; ++n) {
        for (int32_t c = 0; c < 17; ++c) (*stats)[n][c] = in->readByte();
    }
}

inline void loadDroppedItems(BinaryReader *in, int8_t *rowCount,
                             SharedArray<SharedArray<int8_t>> *table) {
    int32_t rows = in->readShort();
    *rowCount = (int8_t)rows;
    int32_t cols = in->readShort();
    *table = makeSharedArray2D<int8_t>(rows, cols);
    for (int32_t r = 0; r < rows; ++r) {
        for (int32_t c = 0; c < cols; ++c) (*table)[r][c] = in->readByte();
    }
}

}

#endif
