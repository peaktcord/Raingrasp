#include "src/common/platform/game_session.hpp"

#include <iostream>

namespace {

int failures = 0;
int destroyedGames = 0;
int destroyedFileServices = 0;
int destroyedSaveServices = 0;

void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class TestFiles : public platform::FileLayer {
public:
    bool read(const std::string &, std::vector<uint8_t> *) override { return false; }
    std::string describe() const override { return "test"; }
};

class TestSaves : public platform::SaveStore {
public:
    bool load(const std::string &, std::vector<std::vector<uint8_t>> *) override {
        return false;
    }
    bool save(const std::string &,
              const std::vector<std::vector<uint8_t>> &) override {
        return true;
    }
    bool remove(const std::string &) override { return false; }
    std::vector<std::string> list() override { return {}; }
    int64_t lastModified(const std::string &) override { return 0; }
};

class OwnedFiles : public TestFiles {
public:
    ~OwnedFiles() override { ++destroyedFileServices; }
};

class OwnedSaves : public TestSaves {
public:
    ~OwnedSaves() override { ++destroyedSaveServices; }
};

struct TestGame {
    explicit TestGame(platform::PlatformContext *context)
        : constructionContext(context) {}
    ~TestGame() { ++destroyedGames; }

    platform::PlatformContext *constructionContext;
};

}  // namespace

int main() {
    TestFiles firstFiles;
    TestFiles secondFiles;
    TestSaves firstSaves;
    TestSaves secondSaves;
    {
        platform::GameSession<TestGame> first(&firstFiles, &firstSaves);
        platform::GameSession<TestGame> second(&secondFiles, &secondSaves);

        TestGame *firstGame = first.createGame();
        TestGame *secondGame = second.createGame();
        check(first.game() == firstGame && second.game() == secondGame,
              "sessions own their translated games");
        check(firstGame->constructionContext == &first.context() &&
                  secondGame->constructionContext == &second.context(),
              "games construct inside their session context");
        check(first.context().fileSystem() == &firstFiles &&
                  first.context().saveStore() == &firstSaves &&
                  second.context().fileSystem() == &secondFiles &&
                  second.context().saveStore() == &secondSaves,
              "sessions compose independent resource and save services");
        check(&first.context() != &second.context(),
              "sessions expose independent contexts without active selection");
        check(destroyedGames == 0, "games live for their session lifetime");
    }

    check(destroyedGames == 2, "session destruction releases translated games");
    {
        platform::GameSession<TestGame> owned(
            std::make_unique<OwnedFiles>(), std::make_unique<OwnedSaves>());
        owned.createGame();
        check(owned.context().fileSystem() != nullptr &&
                  owned.context().saveStore() != nullptr,
              "session binds services transferred into its ownership");
    }
    check(destroyedGames == 3 && destroyedFileServices == 1 &&
              destroyedSaveServices == 1,
          "session destruction releases its game and owned services");
    if (failures == 0) {
        std::cout << "game session: all checks passed\n";
    }
    return failures == 0 ? 0 : 1;
}
