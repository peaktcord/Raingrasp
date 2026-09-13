#ifndef STORMHOLD_CUS_IMAGE_HPP
#define STORMHOLD_CUS_IMAGE_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"
#include "src/common/render/sprite.hpp"

namespace stormhold {

class CusImage : public render::Sprite {
public:
    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t stride_ = 0;
    bool hasTransparency_ = false;
    int16_t transparentColour_ = 0;
    int16_t transparentIndex_ = -1;
    SharedArray<int16_t> pixels_;
    static SharedArray<int16_t> palette_;

    CusImage() {}
    static CusImage *load(platform::PlatformContext *context, const std::string &string);
    int32_t width() const override;
    int32_t height() const override;
    void draw(Graphics *graphics, int32_t x, int32_t y, int32_t manipulation = 0) override;
    static std::string ensureLeadingSlash(const std::string &string);
    static int32_t readInt32BE(BinaryReader *inputStream);
    static int16_t readInt16BE(BinaryReader *inputStream);
    static void initializeStatics();
};

}
#endif
