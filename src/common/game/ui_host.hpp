#ifndef COMMON_GAME_UI_HOST_HPP
#define COMMON_GAME_UI_HOST_HPP

#include "src/common/game/display_target.hpp"
#include "src/common/game/widget_canvas.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

class UIWidget;

namespace game {

struct Profile;

struct SplashArt {
    Image *top = nullptr;
    Image *bottom = nullptr;
    Image *carrierLogo = nullptr;
    Image *publisherLogo = nullptr;
};

class UiHost {
public:
    virtual ~UiHost() = default;

    virtual const Profile &profile() const = 0;

    virtual WidgetCanvas *uiCanvas() = 0;
    virtual UIWidget *currentUi() = 0;
    virtual CommandListener *commandListener() = 0;
    virtual void showDisplayable(DisplayTarget target) = 0;
    virtual DisplayTarget errorForm() = 0;

    virtual void splashStarted(UIWidget *splash) = 0;
    virtual bool showingCarrierLogo() const = 0;
    virtual bool showingSplash() const = 0;
    virtual void setShowingCarrierLogo(bool on) = 0;
    virtual void setShowingSplash(bool on) = 0;
    virtual SplashArt splashArt() const = 0;
    virtual void releaseSplashArt() = 0;
};

}

#endif
