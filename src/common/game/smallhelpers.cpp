#include "src/common/game/smallhelpers.hpp"

#include <stdexcept>

#include "src/common/game/spells.hpp"

namespace smallhelpers {

bool isWalkable(int8_t tile) {
    if ((tile & WALL) != 0) return false;
    if ((tile & CHAMPION) != 0) return false;
    return (tile & MONSTER) == 0;
}

SharedArray<std::string> readStringTable(BinaryReader *in) {
    int32_t count = in->readShort();
    SharedArray<std::string> out(count);
    for (int32_t n = 0; n < count; ++n) out[n] = in->readUTF();
    return out;
}

SharedArray<std::string> readNpcMessages(int32_t npc, int32_t expected, BinaryReader *in) {
    int32_t count = in->readInt();
    if (count != expected) {
        platform::writeLogLine("ERROR: unexpected number of dialogue messages for npc " +
                               std::to_string(npc));
        throw std::runtime_error("Error in readNPCMessages: npc is " + std::to_string(npc));
    }

    SharedArray<std::string> out(count);
    for (int32_t n = 0; n < count; ++n) out[n] = in->readUTF();
    return out;
}

int32_t saveSize(bool full) { return full ? 400 : 200; }

std::string describeSpell(const SharedArray<std::string> &skillNames, int32_t spell) {
    const Spell *s = Spell::all_[spell];
    std::string out = s->name_ + '\n';
    out = out + skillNames[s->skill_] + '\n';
    out += "Cost: " + std::to_string((int32_t)s->magickaCost_) + '\n';
    out = out + s->description_;
    return out;
}

}
