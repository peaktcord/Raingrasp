#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
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
#include "src/dawnstar/bot_traitor.hpp"
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

bool createCharacter(Game *game) {
    game->startApplication();
    if (game->splashUI_ == nullptr) return false;
    headless::finishSplash(game->splashUI_);

    const int32_t flow[] = {
        KEY_SOFT_RIGHT,
        KEY_SOFT_RIGHT,
        KEY_DOWN,
        KEY_SOFT_RIGHT,
        KEY_SOFT_RIGHT,
    };
    for (int32_t code : flow) press(game, code);
    if (!host::completeNameForm(game->display_, "Bot")) return false;
    for (int n = 0; n < 3; ++n) press(game, KEY_SOFT_RIGHT);
    return game->gameCanvas_ != nullptr && game->character_ != nullptr;
}

}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <resource-dir> --deduce|--clear|--beat\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[2];
    const bool wantClear = mode == "--clear" || mode == "--beat";
    const bool wantDeduce = mode == "--deduce" || mode == "--beat";
    const bool wantBeat = mode == "--beat";

    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-play"
                                         : std::string("saves/rms-play"));

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
    check(createCharacter(&game), "a character was created and reached the dungeon");
    if (failures != 0) return 1;

    dawnstar::DawnstarBot bot(&game, &g_now, kTickMs);
    Player *player = bot.player();

    if (wantClear) {
        const std::vector<bot::SweepResult> swept = bot.sweepAllDungeons();
        int32_t tiles = 0, kills = 0, chests = 0;
        for (const bot::SweepResult &s : swept) {
            tiles += s.tilesWalked;
            kills += s.monstersKilled;
            chests += s.chestsOpened;
        }
        std::printf("swept %d dungeons: %d tiles, %d kills, %d chests\n",
                    (int)swept.size(), tiles, kills, chests);
        std::printf("reachable on foot from the camp: %d of %d dungeons\n",
                    (int)swept.size(), (int)worldstate::DungeonRegistry::kDungeonCount);
        check(swept.size() == worldstate::DungeonRegistry::kDungeonCount,
              "every dungeon was reached and swept");
        check(tiles > 9000, "the sweep walked the whole world");
        check(kills > 100, "the sweep fought its way through");
        check(chests > 100, "the sweep looted as it went");
        std::printf("levels taken: %d\n", bot.levelUpsTaken());
        check(bot.levelUpsTaken() > 0, "the sweep earned and spent level-ups");
    }

    if (wantDeduce) {
        const dawnstar::Deduction found = dawnstar::deduceTraitor(bot);
        std::printf("deduction: %d questions, %d lies, accusing NPC %d\n",
                    found.questionsAsked, found.liesHeard, found.accused);
        check(found.solved, "a lie was caught, so the traitor is identified");
        if (!found.solved) return 1;

        check(found.accused == dawnstar::ext(player).traitorId_,
              "the NPC caught lying is in fact the traitor");
        check(dawnstar::accuse(bot, found.accused), "the game accepted the accusation");
        check(dawnstar::ext(player).traitorRevealed_, "the traitor is revealed");
    }

    if (wantBeat) {
        check(dawnstar::ext(player).oracleIndex_ == 1, "the countdown started");

        Monster *boss = nullptr;
        Monster decoded;
        const std::size_t dungeonIndex = (std::size_t)(player->dungeonId_ - 1);
        for (int tick = 0; tick < kBossTick * 12 && boss == nullptr; ++tick) {
            bot.heal();
            bot.tick();
            std::vector<std::pair<int32_t, int32_t>> arrivals;
            for (const worldstate::MonsterRecord &record :
                 game.worldState().monsters.at(dungeonIndex)) {
                Monster *m = Monster::fromRecord(&decoded, record, player->dungeon());
                if (m == nullptr) continue;
                if (m->type_ == kBossType) {
                    boss = m;
                    break;
                }
                arrivals.emplace_back(m->gridX_, m->gridY_);
            }
            if (boss != nullptr) break;
            for (const auto &at : arrivals) {
                bot.walkTo(at.first, at.second);
                for (const int32_t facing : {1, 2, 3, 4}) {
                    player->facing_ = (int8_t)facing;
                    player->refreshSurroundings();
                    bot.fightAhead();
                }
            }
        }
        std::printf("oracle countdown reached %d in dungeon %d\n",
                    dawnstar::ext(player).oracleIndex_, (int)player->dungeonId_);
        check(boss != nullptr, "the countdown sent the boss");
        check(dawnstar::ext(player).oracleIndex_ >= kBossTick, "the countdown ran to the end");
        if (boss == nullptr) return 1;

        Monster target = *boss;
        bot.walkTo(target.gridX_, target.gridY_);
        check(bot.fightAhead(2000), "the boss was killed");
        bot.tick();

        check(screenOf(&game) == uistate::SCREEN_END_OF_GAME,
              "killing the boss reached the Victory screen");
    }

    if (failures == 0) std::printf("%s: OK\n", mode.c_str());
    return failures == 0 ? 0 : 1;
}
