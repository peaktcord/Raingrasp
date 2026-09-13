#include "src/common/render/render.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "src/common/render/s60font.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "tools/3p/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tools/3p/stb_image_write.h"

namespace render {

const S60Face *faceFor(Font *font) {
    if (font == nullptr) {
        return &kSmallBold;
    }
    int32_t size = font->getSize();
    int32_t style = font->getStyle();
    if (size == 16) {
        return (style & 2) ? &kLargeItalic : &kLargeBold;
    }
    if (size == 0) {
        return (style & 1) ? &kMediumBold : &kMediumPlain;
    }
    return (style & 1) ? &kSmallBold : &kSmallPlain;
}

static const S60Glyph *glyphFor(const S60Face *face, unsigned char code) {
    if (code < kS60FirstCode || code > kS60LastCode) {
        code = '?';
    }
    return &face->glyphs[code - kS60FirstCode];
}

int32_t s60StringWidth(Font *font, const char *text, int length) {
    const S60Face *face = faceFor(font);
    int32_t width = 0;
    for (int n = 0; n < length; ++n) {
        width += glyphFor(face, (unsigned char)text[n])->advance;
    }
    return width;
}

int32_t glyphAdvance(Font *font) {
    return font == nullptr ? 6 : font->charWidth('m');
}

int32_t lineHeight(Font *font) {
    return font == nullptr ? 12 : font->getHeight();
}

Surface *surfaceFor(Image *image) {
    if (image == nullptr) {
        return nullptr;
    }
    if (image->backend() != nullptr) {
        return static_cast<Surface *>(image->backend());
    }
    Surface *surface = new Surface();
    const std::vector<uint8_t> &encoded = image->encodedBytes();
    if (!encoded.empty()) {
        int w = 0;
        int h = 0;
        int channels = 0;
        stbi_uc *rgba = stbi_load_from_memory(encoded.data(), (int)encoded.size(), &w, &h,
                                              &channels, 4);
        if (rgba != nullptr) {
            surface->width = w;
            surface->height = h;
            surface->pixels.resize((size_t)w * (size_t)h);
            for (size_t n = 0; n < surface->pixels.size(); ++n) {
                uint32_t r = rgba[n * 4 + 0];
                uint32_t g = rgba[n * 4 + 1];
                uint32_t b = rgba[n * 4 + 2];
                uint32_t a = rgba[n * 4 + 3];
                surface->pixels[n] = (a << 24) | (r << 16) | (g << 8) | b;
            }
            stbi_image_free(rgba);
        }
    }
    if (surface->empty()) {
        int32_t w = image->getWidth();
        int32_t h = image->getHeight();
        if (w > 0 && h > 0) {
            *surface = Surface(w, h);
        }
    }
    image->setBackend(surface);
    return surface;
}

SoftGraphics::SoftGraphics(Surface *target, Context *context)
    : target_(target), context_(context) {
    resetClip();
}

void SoftGraphics::bind(Surface *target) {
    target_ = target;
    resetClip();
}

void SoftGraphics::setOrigin(int32_t ox, int32_t oy) {
    ox_ = ox;
    oy_ = oy;
}

void SoftGraphics::resetClip() {
    cx_ = 0;
    cy_ = 0;
    cw_ = target_ != nullptr ? target_->width : 0;
    ch_ = target_ != nullptr ? target_->height : 0;
}

void SoftGraphics::onClip() {
    if (target_ == nullptr) {
        cx_ = cy_ = cw_ = ch_ = 0;
        return;
    }
    int64_t x = (int64_t)Graphics::clipX_ + ox_;
    int64_t y = (int64_t)Graphics::clipY_ + oy_;
    int64_t x1 = x + (int64_t)Graphics::clipW_;
    int64_t y1 = y + (int64_t)Graphics::clipH_;
    int64_t x0 = std::max<int64_t>(0, x);
    int64_t y0 = std::max<int64_t>(0, y);
    x1 = std::min<int64_t>(target_->width, x1);
    y1 = std::min<int64_t>(target_->height, y1);
    cx_ = (int32_t)x0;
    cy_ = (int32_t)y0;
    cw_ = (int32_t)(x1 > x0 ? x1 - x0 : 0);
    ch_ = (int32_t)(y1 > y0 ? y1 - y0 : 0);
}

void SoftGraphics::blendPixel(int32_t x, int32_t y, uint32_t argb) {
    if (x < cx_ || y < cy_ || x >= cx_ + cw_ || y >= cy_ + ch_) {
        return;
    }
    uint32_t alpha = argb >> 24;
    if (alpha == 0) {
        return;
    }
    uint32_t *pixel = target_->row(y) + x;
    if (alpha == 255) {
        *pixel = argb;
        return;
    }
    uint32_t dst = *pixel;
    uint32_t inv = 255 - alpha;
    uint32_t r = (((argb >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * inv) / 255;
    uint32_t g = (((argb >> 8) & 0xFF) * alpha + ((dst >> 8) & 0xFF) * inv) / 255;
    uint32_t b = ((argb & 0xFF) * alpha + (dst & 0xFF) * inv) / 255;
    *pixel = 0xFF000000u | (r << 16) | (g << 8) | b;
}

void SoftGraphics::fillSpan(int32_t x, int32_t y, int32_t w, uint32_t argb) {
    if (y < cy_ || y >= cy_ + ch_) {
        return;
    }
    int32_t x0 = std::max(x, cx_);
    int32_t x1 = std::min(x + w, cx_ + cw_);
    uint32_t *row = target_->row(y);
    for (int32_t px = x0; px < x1; ++px) {
        row[px] = argb;
    }
}

static uint32_t opaque(int32_t rgb) {
    return 0xFF000000u | ((uint32_t)rgb & 0x00FFFFFFu);
}

void SoftGraphics::fillRect(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (target_ == nullptr || w <= 0 || h <= 0) {
        return;
    }
    x += ox_; y += oy_;
    uint32_t argb = opaque(getColor());
    for (int32_t py = y; py < y + h; ++py) {
        fillSpan(x, py, w, argb);
    }
}

void SoftGraphics::drawRect(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (target_ == nullptr || w < 0 || h < 0) {
        return;
    }
    x += ox_; y += oy_;
    uint32_t argb = opaque(getColor());
    fillSpan(x, y, w + 1, argb);
    fillSpan(x, y + h, w + 1, argb);
    for (int32_t py = y; py <= y + h; ++py) {
        blendPixel(x, py, argb);
        blendPixel(x + w, py, argb);
    }
}

void SoftGraphics::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t aw, int32_t ah) {
    if (target_ == nullptr || w <= 0 || h <= 0) {
        return;
    }
    x += ox_; y += oy_;
    uint32_t argb = opaque(getColor());
    int32_t rx = std::min(aw / 2, w / 2);
    int32_t ry = std::min(ah / 2, h / 2);
    for (int32_t py = 0; py < h; ++py) {
        int32_t inset = 0;
        int32_t dy = -1;
        if (py < ry) {
            dy = ry - py;
        } else if (py >= h - ry) {
            dy = py - (h - ry - 1);
        }
        if (dy > 0 && rx > 0 && ry > 0) {
            double norm = 1.0 - (double)(dy * dy) / (double)(ry * ry);
            double span = norm > 0.0 ? (double)rx * (1.0 - std::sqrt(norm)) : (double)rx;
            inset = (int32_t)(span + 0.5);
        }
        fillSpan(x + inset, y + py, w - 2 * inset, argb);
    }
}

void SoftGraphics::drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if (target_ == nullptr) {
        return;
    }
    x1 += ox_; y1 += oy_; x2 += ox_; y2 += oy_;
    uint32_t argb = opaque(getColor());
    int32_t dx = std::abs(x2 - x1);
    int32_t dy = -std::abs(y2 - y1);
    int32_t sx = x1 < x2 ? 1 : -1;
    int32_t sy = y1 < y2 ? 1 : -1;
    int32_t err = dx + dy;
    while (true) {
        blendPixel(x1, y1, argb);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

int32_t SoftGraphics::drawGlyph(const S60Face *face, char ch, int32_t x, int32_t top, uint32_t argb) {
    unsigned char code = (unsigned char)ch;
    if (code < kS60FirstCode || code > kS60LastCode) {
        code = '?';
    }
    const S60Glyph &glyph = face->glyphs[code - kS60FirstCode];
    for (int32_t row = 0; row < glyph.height; ++row) {
        uint32_t bits = face->rows[glyph.rowOffset + row];
        for (int32_t col = 0; col < glyph.width; ++col) {
            if (bits & (1u << col)) {
                blendPixel(x + col, top + row, argb);
            }
        }
    }
    return glyph.advance;
}

static void applyAnchor(int32_t anchor, int32_t w, int32_t h, int32_t ascent, int32_t *x, int32_t *y) {
    if (anchor & HCENTER) {
        *x -= w / 2;
    } else if (anchor & RIGHT) {
        *x -= w;
    }
    if (anchor & VCENTER) {
        *y -= h / 2;
    } else if (anchor & BOTTOM) {
        *y -= h;
    } else if (anchor & BASELINE) {
        *y -= ascent;
    }
}

void SoftGraphics::drawString(const std::string &text, int32_t x, int32_t y, int32_t anchor) {
    if (target_ == nullptr) {
        return;
    }
    Font *font = getFont();
    const S60Face *face = faceFor(font);
    const std::string &chars = text;
    int32_t height = lineHeight(font);
    int32_t width = s60StringWidth(font, chars.c_str(), (int)chars.size());

    int32_t ascent = 0;
    for (int32_t n = 0; n <= kS60LastCode - kS60FirstCode; ++n) {
        if (face->glyphs[n].ascent > ascent) {
            ascent = face->glyphs[n].ascent;
        }
    }

    x += ox_; y += oy_;
    applyAnchor(anchor, width, height, ascent, &x, &y);
    uint32_t argb = opaque(getColor());
    int32_t baseline = y + ascent;
    for (size_t n = 0; n < chars.size(); ++n) {
        unsigned char code = (unsigned char)chars[n];
        if (code < kS60FirstCode || code > kS60LastCode) {
            code = '?';
        }
        const S60Glyph &glyph = face->glyphs[code - kS60FirstCode];
        x += drawGlyph(face, (char)code, x, baseline - glyph.ascent, argb);
    }
}

void SoftGraphics::drawChar(char ch, int32_t x, int32_t y, int32_t anchor) {
    drawString(std::string(std::string(1, ch)), x, y, anchor);
}

void SoftGraphics::drawImage(Image *image, int32_t x, int32_t y, int32_t anchor) {
    if (context_ != nullptr && context_->handleImage(this, image, x, y, anchor)) {
        return;
    }
    if (target_ == nullptr || image == nullptr) {
        return;
    }
    Surface *source = surfaceFor(image);
    if (source == nullptr || source->empty()) {
        return;
    }
    x += ox_; y += oy_;
    applyAnchor(anchor, source->width, source->height, source->height, &x, &y);
    for (int32_t sy = 0; sy < source->height; ++sy) {
        int32_t dy = y + sy;
        if (dy < cy_ || dy >= cy_ + ch_) {
            continue;
        }
        const uint32_t *srcRow = source->row(sy);
        for (int32_t sx = 0; sx < source->width; ++sx) {
            blendPixel(x + sx, dy, srcRow[sx]);
        }
    }
}

void SoftGraphics::drawImage(Image *image, int32_t x, int32_t y, int32_t anchor, int32_t manipulation) {
    if (context_ != nullptr && context_->handleImage(this, image, x, y, anchor)) {
        return;
    }
    if ((manipulation & IMAGE_FLIP_HORIZONTAL) == 0) {
        drawImage(image, x, y, anchor);
        return;
    }
    if (target_ == nullptr || image == nullptr) {
        return;
    }
    Surface *source = surfaceFor(image);
    if (source == nullptr || source->empty()) {
        return;
    }
    x += ox_; y += oy_;
    applyAnchor(anchor, source->width, source->height, source->height, &x, &y);
    for (int32_t sy = 0; sy < source->height; ++sy) {
        int32_t dy = y + sy;
        if (dy < cy_ || dy >= cy_ + ch_) {
            continue;
        }
        const uint32_t *srcRow = source->row(sy);
        for (int32_t sx = 0; sx < source->width; ++sx) {
            blendPixel(x + sx, dy, srcRow[source->width - 1 - sx]);
        }
    }
}

void SoftGraphics::drawPixels(const SharedArray<int16_t> &pixels, bool transparency, int32_t offset,
                              int32_t scanlength, int32_t x, int32_t y, int32_t width, int32_t height,
                              int32_t manipulation, int32_t format) {
    if (target_ == nullptr || pixels.isNull() || width <= 0 || height <= 0) {
        return;
    }
    (void)format;
    bool flip = (manipulation & IMAGE_FLIP_HORIZONTAL) != 0;
    x += ox_; y += oy_;
    for (int32_t sy = 0; sy < height; ++sy) {
        int32_t dy = y + sy;
        if (dy < cy_ || dy >= cy_ + ch_) {
            continue;
        }
        for (int32_t sx = 0; sx < width; ++sx) {
            int32_t column = flip ? width - 1 - sx : sx;
            int32_t index = offset + sy * scanlength + column;
            if (index < 0 || index >= pixels.length()) {
                continue;
            }
            uint32_t packed = (uint32_t)(uint16_t)pixels[index];
            uint32_t a = (packed >> 12) & 0xF;
            uint32_t r = (packed >> 8) & 0xF;
            uint32_t g = (packed >> 4) & 0xF;
            uint32_t b = packed & 0xF;
            if (transparency && a == 0) {
                continue;
            }
            uint32_t argb = ((a * 17) << 24) | ((r * 17) << 16) | ((g * 17) << 8) | (b * 17);
            blendPixel(x + sx, dy, argb);
        }
    }
}

Context::Context(Surface *screen) { bind(screen); }

Context::~Context() = default;

void Context::bind(Surface *screen) {
    screen_ = screen;
    if (screenGraphics_ == nullptr) {
        screenGraphics_ = std::make_unique<SoftGraphics>(screen, this);
    } else {
        screenGraphics_->bind(screen);
    }
}

Graphics *Context::screenGraphics() { return screenGraphics_.get(); }

Graphics *Context::createImageGraphics(Image *image) {
    Surface *surface = surfaceFor(image);
    std::unique_ptr<SoftGraphics> &graphics = imageGraphics_[image];
    if (!graphics) {
        graphics = std::make_unique<SoftGraphics>(surface, this);
    } else {
        graphics->bind(surface);
    }
    return graphics.get();
}

void Context::beginPaint() {
    frameMutex_.lock();
    ++frameSerial_;
    if (screenGraphics_ != nullptr) {
        screenGraphics_->resetClip();
    }
}

void Context::endPaint() {
    if (postPaintHook_ != nullptr) {
        postPaintHook_(postPaintContext_);
    }
    frameMutex_.unlock();
}

bool Context::handleImage(SoftGraphics *graphics, Image *image, int32_t x, int32_t y,
                          int32_t anchor) const {
    return imageHook_ != nullptr &&
           imageHook_(imageHookContext_, graphics, image, x, y, anchor);
}

bool writePng(const Surface &surface, const std::string &path) {
    if (surface.empty()) {
        return false;
    }
    std::vector<unsigned char> rgba((size_t)surface.width * (size_t)surface.height * 4);
    for (size_t n = 0; n < surface.pixels.size(); ++n) {
        uint32_t argb = surface.pixels[n];
        rgba[n * 4 + 0] = (unsigned char)((argb >> 16) & 0xFF);
        rgba[n * 4 + 1] = (unsigned char)((argb >> 8) & 0xFF);
        rgba[n * 4 + 2] = (unsigned char)(argb & 0xFF);
        rgba[n * 4 + 3] = (unsigned char)((argb >> 24) & 0xFF);
    }
    return stbi_write_png(path.c_str(), surface.width, surface.height, 4, rgba.data(),
                          surface.width * 4) != 0;
}

bool readPng(const std::string &path, Surface *out) {
    if (out == nullptr) {
        return false;
    }
    std::FILE *fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        return false;
    }
    std::fseek(fp, 0, SEEK_END);
    long size = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> encoded((size_t)(size < 0 ? 0 : size));
    if (!encoded.empty()) {
        size_t got = std::fread(encoded.data(), 1, encoded.size(), fp);
        encoded.resize(got);
    }
    std::fclose(fp);
    if (encoded.empty()) {
        return false;
    }
    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc *rgba =
        stbi_load_from_memory(encoded.data(), (int)encoded.size(), &w, &h, &channels, 4);
    if (rgba == nullptr) {
        return false;
    }
    out->width = w;
    out->height = h;
    out->pixels.resize((size_t)w * (size_t)h);
    for (size_t n = 0; n < out->pixels.size(); ++n) {
        uint32_t r = rgba[n * 4 + 0];
        uint32_t g = rgba[n * 4 + 1];
        uint32_t b = rgba[n * 4 + 2];
        uint32_t a = rgba[n * 4 + 3];
        out->pixels[n] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    stbi_image_free(rgba);
    return true;
}

}
