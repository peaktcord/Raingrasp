#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/common/host/registry.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/cli.hpp"
#include "src/common/replay/replay.hpp"

namespace {

struct HostProbe : replay::Probe {
    explicit HostProbe(std::unique_ptr<host::GameHost> game)
        : screen_(176, 208), renderer_(&screen_), game_(std::move(game)) {}

    platform::PlatformContext *platformContext() override { return &context_; }

    bool boot(const std::string &resourceDir, const std::string &saveDir) override {
        Resources::setRoot(resourceDir);
        SaveRecordFiles::setRoot(saveDir);
        context_.installFileSystem(platform::defaultContext()->fileSystem());
        context_.installSaveStore(platform::defaultContext()->saveStore());
        context_.installRenderServices(&renderer_);

        game_->boot(&context_, &renderer_, &screen_);
        if (!game_->hasSplash()) {
            std::fprintf(stderr, "probe: no splash after startApplication\n");
            return false;
        }
        for (int guard = 0; guard < 200; ++guard) {
            if (!game_->stepSplash(500)) break;
        }
        return true;
    }

    void key(int32_t code, bool press) override {
        if (press) {
            game_->keyPressed(code);
        } else {
            game_->keyReleased(code);
        }
        game_->drainPendingWork();
    }

    void step(int64_t) override {
        host::completeNameForm(game_->display(), "Tester");
        game_->drainPendingWork();
        if (game_->canvasRunning()) {
            int64_t period = 0;
            game_->tick(&period);
        }
        game_->drainPendingWork();
    }

    void hashState(replay::Hasher &out) override { game_->hashState(out); }
    std::string describe() override { return game_->describe(); }

    render::Surface screen_;
    render::Context renderer_;
    platform::PlatformContext context_;
    std::unique_ptr<host::GameHost> game_;
};

}

int main(int argc, char **argv) {
    std::string id;
    std::vector<char *> rest;
    for (int n = 0; n < argc; ++n) {
        if (n + 1 < argc && std::strcmp(argv[n], "--game") == 0) {
            id = argv[++n];
            continue;
        }
        rest.push_back(argv[n]);
    }
    std::unique_ptr<host::GameHost> game = host::createHost(id);
    if (game == nullptr) {
        std::fprintf(stderr, "usage: %s --game dawnstar|stormhold <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }
    std::string saveDir = std::string("saves/rms-replay-") + game->id();
    HostProbe probe(std::move(game));
    replay::Script script = probe.game_->replayScript();
    return replay::runCli(probe, script, saveDir.c_str(), (int)rest.size(), rest.data());
}
