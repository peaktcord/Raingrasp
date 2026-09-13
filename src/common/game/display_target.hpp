#ifndef COMMON_GAME_DISPLAY_TARGET_HPP
#define COMMON_GAME_DISPLAY_TARGET_HPP

#include <cstddef>
#include <variant>

class Displayable;
class UIWidget;

namespace game {

class DisplayTarget {
public:
    DisplayTarget() = default;
    DisplayTarget(std::nullptr_t) {}
    DisplayTarget(UIWidget *widget) : value_(widget) {}
    DisplayTarget(Displayable *displayable) : value_(displayable) {}

    UIWidget *widget() const {
        UIWidget *const *value = std::get_if<UIWidget *>(&value_);
        return value == nullptr ? nullptr : *value;
    }
    Displayable *displayable() const {
        Displayable *const *value = std::get_if<Displayable *>(&value_);
        return value == nullptr ? nullptr : *value;
    }
    bool isNull() const { return widget() == nullptr && displayable() == nullptr; }
    bool operator==(std::nullptr_t) const { return isNull(); }
    bool operator!=(std::nullptr_t) const { return !isNull(); }
    bool operator==(UIWidget *other) const { return widget() == other; }

private:
    std::variant<std::monostate, UIWidget *, Displayable *> value_;
};

}

#endif
