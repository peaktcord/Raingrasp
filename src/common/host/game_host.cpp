#include "src/common/host/game_host.hpp"

#include <vector>

namespace host {

void pump(GameHost *game, int64_t nowMs, int64_t *nextTickMs, int64_t *nextSplashMs) {
    if (game == nullptr) return;

    game->drainPendingWork();

    if (game->hasSplash() && nowMs >= *nextSplashMs) {
        *nextSplashMs += 500;
        if (*nextSplashMs <= nowMs) *nextSplashMs = nowMs + 500;
        game->stepSplash(500);
    }

    if (nowMs < *nextTickMs) return;
    if (!game->canvasRunning()) {
        *nextTickMs += 250;
        if (*nextTickMs <= nowMs) *nextTickMs = nowMs + 250;
        return;
    }
    int64_t period = 0;
    TickStatus status = game->tick(&period);
    game->drainPendingWork();
    if (status == TickStatus::Failed) {
        game->requestExit();
        return;
    }

    if (period <= 0) period = 250;
    *nextTickMs += period;
    if (*nextTickMs <= nowMs) *nextTickMs = nowMs + period;
}

bool completeNameForm(Display *display, const char *name) {
    if (display == nullptr) return false;
    Form *form = dynamic_cast<Form *>(display->getCurrent());
    if (form == nullptr || form->size() <= 0) return false;
    for (int32_t n1 = 0; n1 < form->size(); ++n1) {
        if (TextField *field = dynamic_cast<TextField *>(form->get(n1))) {
            field->setString(std::string(name));
        }
    }
    const std::vector<Command *> &commands = form->commands();
    if (form->listener() == nullptr || commands.empty()) return false;
    form->listener()->commandAction(commands[0], form);
    return true;
}

}
