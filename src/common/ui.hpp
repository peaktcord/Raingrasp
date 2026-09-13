#ifndef COMMON_UI_HPP
#define COMMON_UI_HPP

#include <stdexcept>

#include "src/common/platform/platform.hpp"
#include "src/common/runtime.hpp"
#include "src/common/font_metrics.hpp"

class Font {
    int32_t face_, style_, size_;
    Font(int32_t f, int32_t st, int32_t sz) : face_(f), style_(st), size_(sz) {}
public:
    static Font *getFont(int32_t face, int32_t style, int32_t size) {
        static std::vector<Font *> cache;
        for (Font *f : cache) {
            if (f->face_ == face && f->style_ == style && f->size_ == size) return f;
        }
        Font *f = new Font(face, style, size);
        cache.push_back(f);
        return f;
    }
    int32_t getSize() const { return size_; }
    int32_t getStyle() const { return style_; }
    int32_t getFace() const { return face_; }
    int32_t getHeight() const { return size_ == 8 ? 12 : (size_ == 16 ? 17 : 13); }

    const uint8_t *advances() const {
        if (size_ == 16) {
            return (style_ & 2) ? s60metrics::kLargeItalicAdvance
                                : s60metrics::kLargeBoldAdvance;
        }
        if (size_ == 0) {
            return (style_ & 1) ? s60metrics::kMediumBoldAdvance
                                : s60metrics::kMediumPlainAdvance;
        }
        return (style_ & 1) ? s60metrics::kSmallBoldAdvance
                            : s60metrics::kSmallPlainAdvance;
    }

    int32_t charWidth(char c) const {
        unsigned char code = (unsigned char)c;
        if (code < s60metrics::kFirstCode || code > s60metrics::kLastCode) {
            code = '?';
        }
        return advances()[code - s60metrics::kFirstCode];
    }

    int32_t stringWidth(const std::string &s) const {
        return substringWidth(s, 0, (int32_t)s.length());
    }

    int32_t substringWidth(const std::string &s, int32_t off, int32_t len) const {
        const std::string &text = s;
        int32_t width = 0;
        for (int32_t n = 0; n < len; ++n) {
            int32_t at = off + n;
            if (at < 0 || at >= (int32_t)text.size()) {
                break;
            }
            width += charWidth(text[(size_t)at]);
        }
        return width;
    }
};

class Image {
    int32_t w_ = 0, h_ = 0;
    std::vector<uint8_t> data_;
    Image() {}
    void parsePngSize() {
        if (data_.size() >= 24 && data_[1] == 'P' && data_[2] == 'N' && data_[3] == 'G') {
            w_ = (data_[16] << 24) | (data_[17] << 16) | (data_[18] << 8) | data_[19];
            h_ = (data_[20] << 24) | (data_[21] << 16) | (data_[22] << 8) | data_[23];
        }
    }
public:
    static Image *createImage(platform::PlatformContext *context,
                              const std::string &resource);
    static Image *createImage(platform::PlatformContext *context,
                              const SharedArray<int8_t> &data, int32_t off, int32_t len);
    static Image *createImage(const SharedArray<int8_t> &data, int32_t off, int32_t len) {
        Image *img = new Image();
        img->data_.resize((size_t)len);
        for (int32_t i = 0; i < len; ++i) img->data_[(size_t)i] = (uint8_t)data[off + i];
        img->parsePngSize();
        return img;
    }
    static Image *createImage(int32_t w, int32_t h) {
        Image *img = new Image();
        img->w_ = w;
        img->h_ = h;
        return img;
    }
    static Image *createImage(platform::PlatformContext *context, int32_t w, int32_t h);
    int32_t getWidth() const { return w_; }
    int32_t getHeight() const { return h_; }
    class Graphics *getGraphics();
    const std::vector<uint8_t> &encodedBytes() const { return data_; }

    void *backend() const { return backend_; }
    void setBackend(void *p) { backend_ = p; }

private:
    void *backend_ = nullptr;
    platform::RenderServices *renderServices_ = nullptr;
};

enum : int32_t { IMAGE_FLIP_HORIZONTAL = 8192 };

class Graphics {
protected:
    int32_t color_ = 0;
    Font *font_ = Font::getFont(0, 0, 0);
    int32_t clipX_ = 0, clipY_ = 0, clipW_ = 0x7FFFFFFF, clipH_ = 0x7FFFFFFF;
public:
    virtual ~Graphics() = default;
    void setColor(int32_t c) { color_ = c; }
    int32_t getColor() const { return color_; }
    void setFont(Font *f) { font_ = f; }
    Font *getFont() const { return font_; }
    void setClip(int32_t x, int32_t y, int32_t w, int32_t h) {
        clipX_ = x; clipY_ = y; clipW_ = w; clipH_ = h;
        onClip();
    }
    virtual void onClip() {}
    virtual void fillRect(int32_t, int32_t, int32_t, int32_t) {}
    virtual void drawRect(int32_t, int32_t, int32_t, int32_t) {}
    virtual void fillRoundRect(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) {}
    virtual void drawLine(int32_t, int32_t, int32_t, int32_t) {}
    virtual void drawString(const std::string &, int32_t, int32_t, int32_t) {}
    virtual void drawChar(char, int32_t, int32_t, int32_t) {}
    virtual void drawImage(Image *, int32_t, int32_t, int32_t) {}

    virtual void drawImage(Image *, int32_t, int32_t, int32_t, int32_t) {}
    virtual void drawPixels(const SharedArray<int16_t> &, bool, int32_t, int32_t, int32_t, int32_t, int32_t,
                            int32_t, int32_t, int32_t) {}
};

namespace platform {

class RenderServices {
public:
    virtual ~RenderServices() = default;
    virtual Graphics *screenGraphics() = 0;
    virtual Graphics *createImageGraphics(Image *image) = 0;
    virtual void beginPaint() = 0;
    virtual void endPaint() = 0;
};

}

inline Graphics *Image::getGraphics() {
    return renderServices_ != nullptr ? renderServices_->createImageGraphics(this)
                                      : new Graphics();
}

inline Image *Image::createImage(platform::PlatformContext *context,
                                 const std::string &resource) {
    Image *img = new Image();
    if (context == nullptr || !context->readResource(resource, &img->data_)) {
        delete img;
        throw std::runtime_error(std::string("IOException: ") + resource);
    }
    img->renderServices_ = context->renderServices();
    img->parsePngSize();
    return img;
}

inline Image *Image::createImage(platform::PlatformContext *context,
                                 const SharedArray<int8_t> &data, int32_t off, int32_t len) {
    Image *img = createImage(data, off, len);
    img->renderServices_ = context->renderServices();
    return img;
}

inline Image *Image::createImage(platform::PlatformContext *context, int32_t w, int32_t h) {
    Image *img = createImage(w, h);
    img->renderServices_ = context->renderServices();
    return img;
}

class Command {
    std::string label_;
    int32_t type_, priority_;
public:
    Command(const std::string &label, int32_t type, int32_t priority)
        : label_(label), type_(type), priority_(priority) {}
    std::string getLabel() const { return label_; }
};

class Displayable {
public:
    virtual ~Displayable() = default;
    virtual void addCommand(Command *) {}
    virtual void removeCommand(Command *) {}
    virtual void setCommandListener(class CommandListener *) {}
};

struct CommandListener {
    virtual void commandAction(Command *c, Displayable *d) = 0;
    virtual ~CommandListener() {}
};

class Canvas : public Displayable {
    bool dirty_ = false;
    platform::RenderServices *renderServices_ = nullptr;
public:
    explicit Canvas(platform::PlatformContext *context = nullptr)
        : renderServices_(context == nullptr ? nullptr : context->renderServices()) {}
    virtual void paint(Graphics *g) = 0;
    virtual void keyPressed(int32_t) {}
    virtual void keyReleased(int32_t) {}
    virtual void showNotify() {}
    int32_t getWidth() const { return 176; }
    int32_t getHeight() const { return 208; }
    int32_t getGameAction(int32_t keyCode) const {
        switch (keyCode) {
            case -1: return 1;
            case -2: return 6;
            case -3: return 2;
            case -4: return 5;
            case -5: return 8;
            case 50: return 1;
            case 56: return 6;
            case 52: return 2;
            case 54: return 5;
            case 53: return 8;
        }
        return 0;
    }
    void repaint() { dirty_ = true; }
    void serviceRepaints();
};
inline void Canvas::serviceRepaints() {
    if (dirty_) {
        dirty_ = false;
        if (renderServices_ != nullptr) {
            // paint() runs game code and can throw.  The frame lock is not
            // recursive, so leaking it here used to turn the next paint on this
            // thread into a resource_deadlock_would_occur -- thrown out of
            // GameCanvas::tick's own catch block, where nothing could handle
            // it.  Unlock on every path out.
            renderServices_->beginPaint();
            struct EndPaint {
                platform::RenderServices *services;
                ~EndPaint() { services->endPaint(); }
            } endPaint{renderServices_};
            paint(renderServices_->screenGraphics());
        } else {
            static Graphics headless;
            paint(&headless);
        }
    }
}

class Item {
public:
    virtual ~Item() = default;
};

class StringItem : public Item {
public:
    std::string label, text;
    StringItem(const std::string &l, const std::string &t) : label(l), text(t) {}
    std::string getText() const { return text; }
    void setText(const std::string &t) { text = t; }
};

class TextField : public Item {
    std::string text_;
public:
    TextField(const std::string &, const std::string &text, int32_t, int32_t)
        : text_(text) {}
    std::string getString() const { return text_; }
    void setString(const std::string &s) { text_ = s; }
};

class Form : public Displayable {
    std::string title_;
    std::vector<Item *> items_;
    std::vector<Command *> commands_;
    CommandListener *listener_ = nullptr;
public:
    explicit Form(const std::string &title) : title_(title) {}
    Form(const std::string &title, std::initializer_list<Item *> items)
        : title_(title), items_(items) {}
    std::string title() const { return title_; }
    void append(Item *item) { items_.push_back(item); }
    Item *get(int32_t i) const { return items_[(size_t)i]; }
    int32_t size() const { return (int32_t)items_.size(); }
    const std::vector<Command *> &commands() const { return commands_; }
    CommandListener *listener() const { return listener_; }
    void addCommand(Command *command) override { commands_.push_back(command); }
    void removeCommand(Command *command) override {
        for (size_t i = 0; i < commands_.size(); ++i) {
            if (commands_[i] == command) { commands_.erase(commands_.begin() + (ptrdiff_t)i); return; }
        }
    }
    void setCommandListener(CommandListener *l) override { listener_ = l; }
};

class Alert : public Displayable {
    std::string title_;
    std::string message_;
public:
    Alert(const std::string &title, const std::string &message)
        : title_(title), message_(message) {}
    std::string title() const { return title_; }
    std::string message() const { return message_; }
    void setTimeout(int32_t) {}
};

class Display {
    Displayable *current_ = nullptr;
public:
    Display() = default;
    Displayable *getCurrent() const { return current_; }
    void setCurrent(Displayable *d) {
        bool changed = current_ != d;
        current_ = d;
        if (changed) {
            if (Canvas *c = dynamic_cast<Canvas *>(d)) c->showNotify();
        }
    }
};

#endif
