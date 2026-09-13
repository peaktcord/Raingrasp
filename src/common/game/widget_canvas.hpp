#ifndef COMMON_GAME_WIDGET_CANVAS_HPP
#define COMMON_GAME_WIDGET_CANVAS_HPP

#include "src/common/ui.hpp"

namespace game {

class Widget {
public:
    virtual ~Widget() = default;
    virtual void paint(Graphics *graphics) = 0;
    virtual void keyPressed(int32_t keyCode) = 0;
    virtual void addCommand(Command *command) = 0;
    virtual void removeCommand(Command *command) = 0;
    virtual void setCommandListener(CommandListener *listener) = 0;
};

class WidgetCanvas : public Canvas {
public:
    Widget *widget_ = nullptr;

    explicit WidgetCanvas(platform::PlatformContext *context) : Canvas(context) {}

    void paint(Graphics *graphics) override;
    void addCommand(Command *command) override;
    void removeCommand(Command *command) override;
    void setCommandListener(CommandListener *commandListener) override;
    void keyPressed(int32_t n) override;
};

}

#endif
