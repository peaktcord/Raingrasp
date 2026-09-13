#ifndef COMMON_GAME_SAVE_CODEC_HPP
#define COMMON_GAME_SAVE_CODEC_HPP

#include "src/common/runtime.hpp"

class Player;

namespace game {

class World;

class SaveCodec {
public:
    virtual ~SaveCodec() = default;

    virtual SharedArray<int8_t> toBytes(Player &player, bool bl) const = 0;

    virtual Player *fromBytes(const SharedArray<int8_t> &bytes, bool bl) const = 0;

    virtual SharedArray<int8_t> writeOtherState(World &world) const = 0;
    virtual void readOtherState(World &world, const SharedArray<int8_t> &bytes) const = 0;
};

}

#endif
