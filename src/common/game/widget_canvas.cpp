#include "src/common/game/widget_canvas.hpp"

namespace game {

void WidgetCanvas::paint(Graphics *graphics) {
    if (this->widget_ != nullptr) {
        this->widget_->paint(graphics);
    }
}

void WidgetCanvas::addCommand(Command *command) {
    if (this->widget_ != nullptr) {
        this->widget_->addCommand(command);
    }
}

void WidgetCanvas::removeCommand(Command *command) {
    if (this->widget_ != nullptr) {
        this->widget_->removeCommand(command);
    }
}

void WidgetCanvas::setCommandListener(CommandListener *commandListener) {
    if (this->widget_ != nullptr) {
        this->widget_->setCommandListener(commandListener);
    }
}

void WidgetCanvas::keyPressed(int32_t n) {
    if (this->widget_ != nullptr) {
        this->widget_->keyPressed(n);
    }
}

}
