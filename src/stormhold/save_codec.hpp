#ifndef STORMHOLD_SAVE_CODEC_HPP
#define STORMHOLD_SAVE_CODEC_HPP

#include "src/common/game/save_codec.hpp"

namespace stormhold {

class PlayerSaveCodec : public game::SaveCodec {
public:
    SharedArray<int8_t> toBytes(Player &player, bool bl) const override;
    Player *fromBytes(const SharedArray<int8_t> &byArray, bool bl) const override;
    SharedArray<int8_t> writeOtherState(game::World &world) const override;
    void readOtherState(game::World &world, const SharedArray<int8_t> &bytes) const override;
};

}
#endif
