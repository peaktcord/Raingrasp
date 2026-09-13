#ifndef COMMON_RENDER_HPP
#define COMMON_RENDER_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/common/ui.hpp"

namespace render {

class Context;

struct Surface {
    int32_t width = 0;
    int32_t height = 0;
    std::vector<uint32_t> pixels;

    Surface() {}
    Surface(int32_t w, int32_t h) : width(w), height(h), pixels((size_t)(w * h), 0xFF000000u) {}
    bool empty() const { return width <= 0 || height <= 0; }
    uint32_t *row(int32_t y) { return pixels.data() + (size_t)y * (size_t)width; }
    const uint32_t *row(int32_t y) const {
        return pixels.data() + (size_t)y * (size_t)width;
    }
};

enum Anchor {
    HCENTER = 1,
    VCENTER = 2,
    LEFT = 4,
    RIGHT = 8,
    TOP = 16,
    BOTTOM = 32,
    BASELINE = 64
};

int32_t glyphAdvance(Font *font);
int32_t lineHeight(Font *font);

struct S60Face;
const S60Face *faceFor(Font *font);
int32_t s60StringWidth(Font *font, const char *text, int length);

class SoftGraphics : public Graphics {
    Surface *target_;
    Context *context_ = nullptr;
    int32_t cx_ = 0, cy_ = 0, cw_ = 0, ch_ = 0;
    int32_t ox_ = 0, oy_ = 0;

    void blendPixel(int32_t x, int32_t y, uint32_t argb);
    void fillSpan(int32_t x, int32_t y, int32_t w, uint32_t argb);
    int32_t drawGlyph(const S60Face *face, char ch, int32_t x, int32_t top, uint32_t argb);

public:
    explicit SoftGraphics(Surface *target, Context *context = nullptr);
    void bind(Surface *target);
    Surface *target() const { return target_; }
    void resetClip();

    void setOrigin(int32_t ox, int32_t oy);
    int32_t originX() const { return ox_; }

    void onClip() override;
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h) override;
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h) override;
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t aw, int32_t ah) override;
    void drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) override;
    void drawString(const std::string &text, int32_t x, int32_t y, int32_t anchor) override;
    void drawChar(char ch, int32_t x, int32_t y, int32_t anchor) override;
    void drawImage(Image *image, int32_t x, int32_t y, int32_t anchor) override;

    void drawImage(Image *image, int32_t x, int32_t y, int32_t anchor, int32_t manipulation) override;
    void drawPixels(const SharedArray<int16_t> &pixels, bool transparency, int32_t offset,
                    int32_t scanlength, int32_t x, int32_t y, int32_t width, int32_t height,
                    int32_t manipulation, int32_t format) override;
};

Surface *surfaceFor(Image *image);

typedef bool (*ImageHook)(void *context, SoftGraphics *graphics, Image *image,
                          int32_t x, int32_t y, int32_t anchor);
typedef void (*PostPaintHook)(void *context);

class Context : public platform::RenderServices {
public:
    explicit Context(Surface *screen = nullptr);
    ~Context() override;
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    void bind(Surface *screen);
    Graphics *screenGraphics() override;
    Graphics *createImageGraphics(Image *image) override;
    void beginPaint() override;
    void endPaint() override;

    SoftGraphics *screen() const { return screenGraphics_.get(); }
    Surface *surface() const { return screen_; }
    std::mutex &frameMutex() { return frameMutex_; }
    uint64_t frameSerial() const { return frameSerial_; }
    void setImageHook(ImageHook hook, void *context = nullptr) {
        imageHook_ = hook;
        imageHookContext_ = context;
    }
    void setPostPaint(PostPaintHook hook, void *context = nullptr) {
        postPaintHook_ = hook;
        postPaintContext_ = context;
    }
    bool handleImage(SoftGraphics *graphics, Image *image, int32_t x, int32_t y,
                     int32_t anchor) const;

private:
    Surface *screen_ = nullptr;
    std::unique_ptr<SoftGraphics> screenGraphics_;
    std::unordered_map<Image *, std::unique_ptr<SoftGraphics>> imageGraphics_;
    std::mutex frameMutex_;
    uint64_t frameSerial_ = 0;
    ImageHook imageHook_ = nullptr;
    void *imageHookContext_ = nullptr;
    PostPaintHook postPaintHook_ = nullptr;
    void *postPaintContext_ = nullptr;
};

bool writePng(const Surface &surface, const std::string &path);

bool readPng(const std::string &path, Surface *out);

}

#endif
