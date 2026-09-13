#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "src/common/host/registry.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/intake.hpp"
#include "src/common/platform/platform.hpp"

static int smokeMain(int argc, char **argv) {
    if (argc >= 2 && (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)) {
        std::fprintf(stderr, "usage: %s [--game dawnstar|stormhold] [<dir>|<game.jar>] [save-dir]\n",
                     argv[0]);
        return 0;
    }

    std::string saveDir;
    platform::intake::Request want;
    {
        bool tookBare = false;
        for (int n = 1; n < argc; ++n) {
            if (std::strcmp(argv[n], "--game") == 0 && n + 1 < argc) {
                want.want = platform::intake::variantFromId(argv[++n]);
            } else if (std::strcmp(argv[n], "--jar") == 0 && n + 1 < argc) {
                want.jarPath = argv[++n];
            } else if (std::strcmp(argv[n], "--data") == 0 && n + 1 < argc) {
                want.dataDir = argv[++n];
            } else if (argv[n][0] != '-') {
                if (!tookBare) {
                    want.bareArg = argv[n];
                    tookBare = true;
                } else {
                    saveDir = argv[n];
                }
            }
        }
        std::string exe = argv[0];
        size_t slash = exe.find_last_of("/\\");
        want.exeDir = slash == std::string::npos ? std::string(".") : exe.substr(0, slash);
    }

    platform::intake::Result data = platform::intake::resolve(want);
    if (data.status != platform::intake::Result::Status::Ready) {
        std::fprintf(stderr, "%s\n", data.message.c_str());
        return 1;
    }
    std::unique_ptr<host::GameHost> game =
        host::createHost(platform::intake::variantId(data.variant));
    if (game == nullptr) {
        std::fprintf(stderr, "SMOKE FAIL: could not tell which game %s holds\n", data.tree.c_str());
        return 1;
    }
    if (saveDir.empty()) saveDir = game->defaultSaveDir();

    Resources::setRoot(data.tree);
    SaveRecordFiles::setRoot(saveDir);
    game->boot(platform::defaultContext(), nullptr, nullptr);

    std::string summary = game->bootSummary();
    std::fputs(summary.c_str(), stdout);
    std::fflush(stdout);
    std::_Exit(summary.find("SMOKE OK") != std::string::npos ? 0 : 1);
}

int main(int argc, char **argv) {
    try {
        return smokeMain(argc, argv);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "SMOKE FAIL: %s\n", e.what());
        return 1;
    }
}
