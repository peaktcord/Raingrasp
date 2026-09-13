#include "src/common/game/savegame.hpp"

namespace savegame {

const char *const kSlotName = "es_gamestate";

std::string slotName() { return std::string(kSlotName); }

SharedArray<int8_t> readBytes(BinaryReader *in, int32_t count) {
    SharedArray<int8_t> bytes(count);
    for (int32_t n = 0; n < count; ++n) {
        bytes[n] = in->readByte();
    }
    return bytes;
}

void writeBytes(BinaryWriter *out, const SharedArray<int8_t> &bytes, int32_t count) {
    for (int32_t n = 0; n < count; ++n) {
        out->writeByte(bytes[n]);
    }
}

}
