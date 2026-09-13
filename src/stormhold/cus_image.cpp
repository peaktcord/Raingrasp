#include "src/stormhold/cus_image.hpp"

#include <stdexcept>

namespace stormhold {

SharedArray<int16_t> CusImage::palette_;

CusImage *CusImage::load(platform::PlatformContext *context, const std::string &string) {
    int32_t n1;
    std::vector<uint8_t> bytes;
    if (context == nullptr ||
        !context->readResource(CusImage::ensureLeadingSlash(string), &bytes)) {
        throw std::runtime_error(std::string("Image ") + string + " is null!");
    }
    BinaryReader inputStream(std::move(bytes));
    CusImage *image = new CusImage();
    image->width_ = CusImage::readInt32BE(&inputStream);
    image->height_ = CusImage::readInt32BE(&inputStream);
    image->stride_ = image->width_;
    int32_t n2 = inputStream.read() & 0xFF;
    image->hasTransparency_ = n2 != 0;
    image->transparentColour_ = CusImage::readInt16BE(&inputStream);
    int32_t n3 = inputStream.read() & 0xFF;
    if (n3 > 255) {
        throw std::runtime_error(std::string("Too many colors in image ") + string);
    }
    int32_t n4 = 0;
    while (n4 < n3) {
        int16_t entry = CusImage::readInt16BE(&inputStream);
        CusImage::palette_[n4] = entry;
        n1 = entry;
        if (image->hasTransparency_ && image->transparentIndex_ < 0 && image->transparentColour_ == n1) {
            image->transparentIndex_ = (int16_t)n4;
        }
        ++n4;
    }
    n1 = image->width_ * image->height_;
    image->pixels_ = SharedArray<int16_t>(n1);
    int32_t n5 = 0;
    while (n5 < n1) {
        int32_t n6 = inputStream.read() & 0xFF;
        int16_t s1 = palette_[n6];
        s1 = image->hasTransparency_ && n6 == image->transparentIndex_ ? (int16_t)(s1 & 0xFFFF0FFF) : (int16_t)(s1 | 0xF000);
        image->pixels_[n5] = s1;
        ++n5;
    }
    return image;
}

int32_t CusImage::width() const {
    return this->width_;
}

int32_t CusImage::height() const {
    return this->height_;
}

void CusImage::draw(Graphics *graphics, int32_t x, int32_t y, int32_t manipulation) {
    graphics->drawPixels(this->pixels_, true, 0, this->stride_, x, y, this->width_,
                         this->height_, manipulation, 4444);
}

std::string CusImage::ensureLeadingSlash(const std::string &string) {
    if (string.rfind("/", 0) == 0) {
        return string;
    }
    return std::string("/") + string;
}

int32_t CusImage::readInt32BE(BinaryReader *inputStream) {
    int32_t n1 = 0;
    int32_t n2 = inputStream->read();
    n1 |= n2 << 24;
    int32_t n3 = inputStream->read();
    n1 |= n3 << 16;
    int32_t n4 = inputStream->read();
    n1 |= n4 << 8;
    int32_t n5 = inputStream->read();
    return n1 |= n5;
}

int16_t CusImage::readInt16BE(BinaryReader *inputStream) {
    int32_t n1 = 0;
    int32_t n2 = inputStream->read();
    n1 |= n2 << 8;
    int32_t n3 = inputStream->read();
    n1 |= n3;
    return (int16_t)(n1 &= 0xFFFF);
}

void CusImage::initializeStatics() {
    palette_ = SharedArray<int16_t>(256);
}

}
