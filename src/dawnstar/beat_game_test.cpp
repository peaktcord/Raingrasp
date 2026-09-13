#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/menuaction.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/host/game_host.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/headless.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/profile.hpp"
#include "src/dawnstar/variant.hpp"

namespace {

int failures = 0;

void check(bool value, const char *message) {
    if (!value) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

const int32_t KEY_DOWN = -2;
const int32_t KEY_SOFT_RIGHT = -7;

const int32_t kBossType = 42;
const int32_t kBossTick = 140;

const int64_t kTickMs = 250;

int64_t g_now = 0;
int64_t botClock() { return g_now; }
void botSleep(int64_t ms) {
    if (ms > 0) g_now += ms;
}

dawnstar::Extension &ext(Player *player) {
    return static_cast<dawnstar::Extension &>(player->extension());
}

void noCanvasLoop(GameCanvas *) {}
void noSplashLoop(UIWidget *) {}

void press(Game *game, int32_t code) {
    if (Canvas *canvas = game->display_ != nullptr
                             ? dynamic_cast<Canvas *>(game->display_->getCurrent())
                             : nullptr) {
        canvas->keyPressed(code);
        canvas->keyReleased(code);
    }
}

int32_t screenOf(Game *game) {
    return game->currentUI_ != nullptr ? game->currentUI_->screenId_ : -1;
}

}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir>\n", argv[0]);
        return 2;
    }
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-beat"
                                         : std::string("saves/rms-beat"));

    render::Surface screen(176, 208);
    render::Context renderer(&screen);
    platform::PlatformContext context;
    context.installFileSystem(platform::defaultContext()->fileSystem());
    context.installSaveStore(platform::defaultContext()->saveStore());
    context.installRenderServices(&renderer);

    g_now = 1000000000000LL;
    context.installClock(botClock);
    context.installSleep(botSleep);

    dawnstar::dawnstar_init_statics(&context);
    Game game(dawnstar::profile(), &context);
    game.setExecutionHooks(headless::runJobInline<Game>, noCanvasLoop, noSplashLoop);
    game.startApplication();
    check(game.splashUI_ != nullptr, "boot produced a splash");
    headless::finishSplash(game.splashUI_);

    const int32_t flow[] = {
        KEY_SOFT_RIGHT,
        KEY_SOFT_RIGHT,
        KEY_DOWN,
        KEY_SOFT_RIGHT,
        KEY_SOFT_RIGHT,
    };
    for (int32_t code : flow) press(&game, code);
    check(host::completeNameForm(game.display_, "Bot"), "the name form was completed");

    for (int n = 0; n < 3; ++n) press(&game, KEY_SOFT_RIGHT);

    check(game.gameCanvas_ != nullptr, "character creation reached the game canvas");
    if (game.gameCanvas_ == nullptr || failures != 0) return 1;
    Player *player = game.character_;
    check(player != nullptr, "a character exists");
    if (player == nullptr) return 1;

    const int32_t traitor = ext(player).traitorId_;
    check(traitor >= 0 && traitor < 4, "the traitor roll produced a valid NPC");

    game.performMenuAction(menuaction::REVEAL_TRAITOR, true);
    check(screenOf(&game) == dawnstar::screens::REVEAL_INTRO,
          "Reveal Traitor opened its intro");

    game.commandAction(UIWidget::cmdOk_, nullptr);
    check(screenOf(&game) == dawnstar::screens::REVEAL_CONFIRM,
          "the intro led to the confirm list");

    game.currentUI_->setSelectedIndex(0);
    game.commandAction(UIWidget::cmdSelect_, nullptr);
    check(screenOf(&game) == dawnstar::screens::REVEAL_WHOM,
          "confirming opened the suspect list");

    game.currentUI_->setSelectedIndex(traitor);
    game.commandAction(UIWidget::cmdSelect_, nullptr);
    check(ext(player).traitorRevealed_, "accusing the right NPC revealed the traitor");
    check(screenOf(&game) == dawnstar::screens::REVEAL_RESULT,
          "the reveal reported its result");

    game.commandAction(UIWidget::cmdOk_, nullptr);
    check(ext(player).oracleIndex_ == 1, "the result screen started the oracle countdown");
    if (failures != 0) return 1;

    // Reveal again. Re-opening the menu after a correct accusation used to run the
    // whole flow a second time: another Star of Frost, and the countdown reset to
    // the start. Now it just shows the verdict and leaves everything alone.
    {
        auto starsOfFrost = [&]() {
            int32_t n = 0;
            for (int32_t i = 0; i < player->itemCount_; ++i) {
                int32_t item = player->inventory_[i] < 0 ? -player->inventory_[i] : player->inventory_[i];
                if (item == 100) ++n;
            }
            return n;
        };
        const int32_t starsBefore = starsOfFrost();
        check(starsBefore == 1, "the first reveal granted exactly one Star of Frost");
        ext(player).oracleIndex_ = 10;  // countdown mid-flight
        game.performMenuAction(menuaction::REVEAL_TRAITOR, true);
        check(screenOf(&game) == dawnstar::screens::REVEAL_RESULT,
              "a second Reveal Traitor shows the verdict instead of the accusation flow");
        game.commandAction(UIWidget::cmdOk_, nullptr);
        check(ext(player).oracleIndex_ == 10, "the second reveal did not reset the countdown");
        check(starsOfFrost() == starsBefore, "the second reveal did not grant another Star of Frost");
        check(ext(player).traitorRevealed_, "the traitor stays revealed");
        ext(player).oracleIndex_ = 1;  // back where the flow left it for the rest of the test
    }
    if (failures != 0) return 1;

    Monster *boss = nullptr;
    Monster decoded;
    const std::size_t dungeonIndex = (std::size_t)(player->dungeonId_ - 1);
    for (int tick = 0; tick < kBossTick * 12 && boss == nullptr; ++tick) {
        player->vitals_[2] = player->vitals_[3];
        g_now += kTickMs;
        game.gameCanvas_->tick();

        std::vector<std::pair<int32_t, int32_t>> clear;
        for (const worldstate::MonsterRecord &record :
             game.worldState().monsters.at(dungeonIndex)) {
            Monster *m = Monster::fromRecord(&decoded, record, player->dungeon());
            if (m == nullptr) continue;
            if (m->type_ == kBossType) {
                boss = m;
                break;
            }
            clear.emplace_back(m->gridX_, m->gridY_);
        }
        if (boss != nullptr) break;
        for (const auto &at : clear) {
            game.removeMonsterAt(player->dungeonId_, at.first, at.second);
        }
    }
    check(boss != nullptr, "the oracle countdown spawned the boss (type 42)");
    check(ext(player).oracleIndex_ >= kBossTick, "the countdown reached the boss tick");
    if (boss == nullptr) return 1;

    Monster target = *boss;
    player->stepCandidate(1);
    target.gridX_ = (int8_t)player->nextX_;
    target.gridY_ = (int8_t)player->nextY_;
    target.hp_ = 0;
    target.store();
    player->vitals_[2] = player->vitals_[3];
    g_now += kTickMs;
    game.gameCanvas_->tick();

    check(screenOf(&game) == uistate::SCREEN_END_OF_GAME,
          "killing the boss reached the end-of-game screen");
    if (failures == 0) {
        std::printf("Dawnstar is beatable: Victory after %d oracle seconds.\n",
                    ext(player).oracleIndex_);
    }
    return failures == 0 ? 0 : 1;
}
