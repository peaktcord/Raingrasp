#include "src/stormhold/host/host.hpp"

#include <string>

#include "src/common/game/items.hpp"
#include "src/common/game/spells.hpp"
#include "src/common/platform/platform.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/stormhold/profile.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/stormhold/variant.hpp"
#include "src/stormhold/input/shortcut_host.hpp"
#include "src/common/game/player.hpp"
#include "src/stormhold/render/raycast_scene.hpp"
#include "src/stormhold/render/widescreen_scene.hpp"
#include "src/common/game/ui_widget.hpp"

namespace stormhold {

namespace {

const int32_t KEY_DOWN = -2;
const int32_t KEY_RIGHT = -4;
const int32_t KEY_SOFT_LEFT = -6;
const int32_t KEY_SOFT_RIGHT = -7;

int32_t g_pendingJob = 0;
bool g_hasPendingJob = false;

void queueHelperJob(Game *, int32_t job) {
    g_pendingJob = job;
    g_hasPendingJob = true;
}

void noCanvasThread(GameCanvas *canvas) { canvas->running_ = true; }
void noSplashThread(UIWidget *) {}

class Host : public host::GameHost {
public:
    const char *id() const override { return "stormhold"; }
    const char *title() const override { return "The Elder Scrolls Travels: Stormhold"; }
    const char *defaultSaveDir() const override { return "saves/rms-stormhold"; }
    const menuaction::Action *optionsRows(int32_t *count) const override {
        *count = profile().optionsRowCount;
        return profile().optionsRows;
    }

    void boot(platform::PlatformContext *context, render::Context *renderer,
              render::Surface *screen) override {
        context_ = context;
        if (renderer != nullptr) {
            widescreen_ = std::make_unique<stormhold_widescreen::Context>(renderer, screen, &game_);
            raycast_ = std::make_unique<stormhold_raycast::Context>(renderer, screen, &game_);
        }
        stormhold_shortcuts::install(&game_);
        stormhold_init_statics(context);

        owned_ = std::make_unique<Game>(profile(), context);
        game_ = owned_.get();
        game_->setExecutionHooks(queueHelperJob, noCanvasThread, noSplashThread);
        game_->startApplication();
        drainPendingWork();
    }

    Display *display() override { return game_->display_; }

    bool hasSplash() override { return game_->splashUI_ != nullptr; }
    bool stepSplash(int64_t ms) override {
        return game_->splashUI_ != nullptr && game_->splashUI_->splashStep(ms);
    }
    bool skipSplash() override {
        return game_->splashUI_ != nullptr && game_->splashUI_->splashSkip();
    }

    void drainPendingWork() override {
        while (g_hasPendingJob) {
            g_hasPendingJob = false;
            game_->runHelperJob(g_pendingJob);
        }
    }

    bool hasCanvas() override { return game_->gameCanvas_ != nullptr; }
    bool canvasRunning() override {
        return game_->gameCanvas_ != nullptr && game_->gameCanvas_->running_;
    }

    host::TickStatus tick(int64_t *periodMs) override {
        GameCanvas *canvas = game_->gameCanvas_;
        *periodMs = 250;
        if (canvas == nullptr || !canvas->running_) return host::TickStatus::Idle;
        GameCanvas::TickStatus status = canvas->tick();
        if (status != GameCanvas::TickStatus::Ran) *periodMs = canvas->pacingDelayMs(status);
        switch (status) {
            case GameCanvas::TickStatus::Ran: return host::TickStatus::Ran;
            case GameCanvas::TickStatus::Paused: return host::TickStatus::Paused;
            case GameCanvas::TickStatus::Stopped: return host::TickStatus::Stopped;
            case GameCanvas::TickStatus::Failed: return host::TickStatus::Failed;
        }
        return host::TickStatus::Ran;
    }

    void requestExit() override { game_->exit(); }
    bool switchRequested() const override { return game_->switchRequested(); }

    Canvas *currentCanvas() {
        return game_->display_ != nullptr ? dynamic_cast<Canvas *>(game_->display_->getCurrent())
                                          : nullptr;
    }
    void keyPressed(int32_t code) override {
        if (Canvas *canvas = currentCanvas()) canvas->keyPressed(code);
    }
    void keyReleased(int32_t code) override {
        if (Canvas *canvas = currentCanvas()) canvas->keyReleased(code);
    }

    void repaintCanvas() override {
        if (Canvas *canvas = currentCanvas()) canvas->repaint();
    }
    void repaintCanvasNow() override {
        if (Canvas *canvas = currentCanvas()) {
            canvas->repaint();
            canvas->serviceRepaints();
        }
    }
    void refreshPortOptionsUI() override { game_->refreshPortOptionsUI(); }

    const shortcuts::Host *shortcutHost() override { return stormhold_shortcuts::host(); }

    void setWideView(bool on) override {
        if (widescreen_ == nullptr) return;
        widescreen_->setEnabled(on);
        raycast_->setEnabled(on);
        widescreen_->setSceneOwnedExternally(on);
    }
    bool wideView() const override { return widescreen_ != nullptr && widescreen_->enabled(); }

    replay::Script replayScript() override {
        replay::Script script;
        script.name = "walk";
        script.seed = 1000000000000LL;
        script.tickMs = 250;

        std::vector<replay::Event> &events = script.events;
        int at = 4;
        const int32_t flow[] = {
            KEY_SOFT_RIGHT,
            KEY_SOFT_RIGHT,
            KEY_DOWN,
            KEY_SOFT_RIGHT,
            KEY_SOFT_RIGHT,
            KEY_SOFT_RIGHT,
            KEY_SOFT_RIGHT,
        };
        for (int32_t code : flow) {
            events.push_back({at, code, true});
            events.push_back({at + 1, code, false});
            at += 4;
        }

        const int32_t walk[] = {50, 50, KEY_RIGHT, 50, 54, KEY_RIGHT, 50, 56};
        at += 4;
        for (int32_t code : walk) {
            events.push_back({at, code, true});
            events.push_back({at + 1, code, false});
            at += 5;
        }
        events.push_back({at, 42, true});
        events.push_back({at + 1, 42, false});
        at += 5;
        events.push_back({at, 55, true});
        events.push_back({at + 1, 55, false});
        at += 5;
        events.push_back({at, KEY_SOFT_LEFT, true});
        events.push_back({at + 1, KEY_SOFT_LEFT, false});
        at += 5;
        script.ticks = at + 10;
        return script;
    }

    void hashState(replay::Hasher &out) override {
        out.i32(game_->currentUI_ != nullptr ? game_->currentUI_->screenId_ : -1);

        GameCanvas *canvas = game_->gameCanvas_;
        out.i32(canvas != nullptr ? 1 : 0);
        if (canvas != nullptr) {
            out.i32(canvas->running_ ? 1 : 0);
            out.i32((int32_t)canvas->screenState_);
            out.i32((int32_t)canvas->campState_);
            out.i32(canvas->mapMode_);
            out.i32(canvas->stateChanged_ ? 1 : 0);
        }

        Player *player = game_->character_;
        out.i32(player != nullptr ? 1 : 0);
        if (player != nullptr) {
            out.i32((int32_t)player->dungeonId_);
            out.i32((int32_t)player->gridX_);
            out.i32((int32_t)player->gridY_);
            out.i32((int32_t)player->facing_);
            out.i32(player->gold_);
            out.i32((int32_t)player->itemCount_);
            for (int32_t n1 = 0; n1 < player->vitals_.length(); ++n1) {
                out.i32((int32_t)player->vitals_[n1]);
            }
            for (int32_t n1 = 0; n1 < player->attributes_.length(); ++n1) {
                out.i32((int32_t)player->attributes_[n1]);
            }
        }
    }

    std::string describe() override {
        std::string out;
        out += "screen=";
        out += std::to_string(game_->currentUI_ != nullptr ? (int)game_->currentUI_->screenId_ : -1);
        GameCanvas *canvas = game_->gameCanvas_;
        out += " canvas=";
        out += (canvas != nullptr && canvas->running_) ? "run" : "-";
        Player *player = game_->character_;
        if (player != nullptr) {
            out += " pos=";
            out += std::to_string((int)player->gridX_) + "," + std::to_string((int)player->gridY_);
            out += " dir=" + std::to_string((int)player->facing_);
            out += " dung=" + std::to_string((int)player->dungeonId_);
        }
        return out;
    }

    std::string bootSummary() override {
        if (game_->splashUI_ == nullptr || game_->splashUI_->progressPercent_ < 100) {
            return "SMOKE FAIL: appload did not complete\n";
        }
        char line[256];
        std::string out = "--- smoke summary ---\n";
        std::snprintf(line, sizeof(line), "items: %d (categories: %d)\n", (int)Items::count_,
                      (int)Items::categoryCount_);
        out += line;
        std::snprintf(line, sizeof(line), "spells: %d\n", (int)Spell::count_);
        out += line;
        std::snprintf(line, sizeof(line), "classes: %d, skills: %d\n", (int)Player::classCount_,
                      (int)Player::skillNames_.length());
        out += line;
        std::snprintf(line, sizeof(line), "dungeons: %d\n", (int)game_->dungeons_.size());
        out += line;
        std::snprintf(line, sizeof(line), "dungeon 2 name: %s  size %dx%d\n",
                      game_->dungeons_.as<Dungeon>(1)->name()[0].c_str(),
                      (int)game_->dungeons_[1]->width_, (int)game_->dungeons_[1]->height_);
        out += line;
        for (int di = 1; di < 37; di += 12) {
            Dungeon *dg = game_->dungeons_.as<Dungeon>((std::size_t)di);
            unsigned long hash = 1469598103u;
            for (int x = 0; x < dg->width_; ++x) {
                for (int y = 0; y < dg->height_; ++y) {
                    hash = (hash ^ (unsigned char)dg->tiles_[x][y]) * 16777619u;
                }
            }
            std::snprintf(line, sizeof(line), "dungeon %d layout digest: %08lx (rooms %d)\n", di + 1,
                          hash & 0xFFFFFFFFul, (int)dg->rooms_.size());
            out += line;
        }
        out += "SMOKE OK\n";
        return out;
    }

private:
    platform::PlatformContext *context_ = nullptr;
    Game *game_ = nullptr;
    std::unique_ptr<Game> owned_;
    std::unique_ptr<stormhold_widescreen::Context> widescreen_;
    std::unique_ptr<stormhold_raycast::Context> raycast_;
};

}

std::unique_ptr<host::GameHost> makeHost() { return std::make_unique<Host>(); }

}
