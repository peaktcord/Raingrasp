#ifndef COMMON_RENDER_SPRITE_HPP
#define COMMON_RENDER_SPRITE_HPP

#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

namespace render {

class Sprite {
public:
    virtual ~Sprite() = default;
    virtual int32_t width() const = 0;
    virtual int32_t height() const = 0;
    virtual void draw(Graphics *graphics, int32_t x, int32_t y, int32_t manipulation = 0) = 0;
};

class ImageSprite : public Sprite {
public:
    explicit ImageSprite(Image *image) : image_(image) {}
    Image *image() const { return image_; }
    int32_t width() const override { return image_->getWidth(); }
    int32_t height() const override { return image_->getHeight(); }
    void draw(Graphics *graphics, int32_t x, int32_t y, int32_t manipulation = 0) override {
        if (manipulation == 0) {
            graphics->drawImage(image_, x, y, 20);
            return;
        }
        graphics->drawImage(image_, x, y, 20, manipulation);
    }

private:
    Image *image_;
};

}

#endif
