#ifndef COMMON_PLATFORM_GAME_SESSION_HPP
#define COMMON_PLATFORM_GAME_SESSION_HPP

#include <memory>
#include <utility>

#include "src/common/platform/platform.hpp"

namespace platform {

template <typename Game>
class GameSession {
public:
    GameSession(FileLayer *fileSystem, SaveStore *saveStore) {
        context_.installFileSystem(fileSystem);
        context_.installSaveStore(saveStore);
    }

    GameSession(std::unique_ptr<FileLayer> fileSystem,
                std::unique_ptr<SaveStore> saveStore)
        : ownedFileSystem_(std::move(fileSystem)),
          ownedSaveStore_(std::move(saveStore)) {
        context_.installFileSystem(ownedFileSystem_.get());
        context_.installSaveStore(ownedSaveStore_.get());
    }

    GameSession(const GameSession &) = delete;
    GameSession &operator=(const GameSession &) = delete;

    template <typename... Args>
    Game *createGame(Args &&...args) {
        game_ = std::make_unique<Game>(&context_, std::forward<Args>(args)...);
        return game_.get();
    }

    Game *game() const { return game_.get(); }
    PlatformContext &context() { return context_; }
    const PlatformContext &context() const { return context_; }

private:
    std::unique_ptr<FileLayer> ownedFileSystem_;
    std::unique_ptr<SaveStore> ownedSaveStore_;
    PlatformContext context_;
    std::unique_ptr<Game> game_;
};

}

#endif
