#ifndef COMMON_GAME_SAVEGAME_HPP
#define COMMON_GAME_SAVEGAME_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"
#include "src/common/save_records.hpp"

namespace savegame {

extern const char *const kSlotName;

std::string slotName();

SharedArray<int8_t> readBytes(BinaryReader *in, int32_t count);

void writeBytes(BinaryWriter *out, const SharedArray<int8_t> &bytes, int32_t count);

}

#endif
