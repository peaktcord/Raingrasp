#ifndef DAWNSTAR_WIDESCREEN_SCENE_HPP
#define DAWNSTAR_WIDESCREEN_SCENE_HPP

#include <memory>

#include "src/common/render/widescreen.hpp"

class Game;
namespace dawnstar_widescreen {

class Context {
public:
    struct Impl;
    Context(render::Context *renderer, render::Surface *screen, Game **gameSlot);
    ~Context();
    void setEnabled(bool on);
    bool enabled() const;
    void setSceneOwnedExternally(bool owned);
private:
    std::unique_ptr<Impl> impl_;
    std::unique_ptr<widescreen::Context> widescreen_;
};

}

#endif
