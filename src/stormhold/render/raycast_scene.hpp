#ifndef STORMHOLD_RAYCAST_SCENE_HPP
#define STORMHOLD_RAYCAST_SCENE_HPP

#include <memory>

#include "src/common/render/raycast.hpp"

class Game;
namespace stormhold_raycast {

class Context {
public:
    struct Impl;
    Context(render::Context *renderer, render::Surface *screen, Game **gameSlot);
    ~Context();
    void setEnabled(bool on);
    bool enabled() const;
    bool ownsCurrentFrame() const;
private:
    std::unique_ptr<Impl> impl_;
    std::unique_ptr<raycast::Context> raycast_;
};

}

#endif
