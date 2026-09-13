#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "src/common/bot/bot.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/world_state.hpp"
#include "src/common/host/game_host.hpp"
#include "src/common/save_records.hpp"
#include "src/common/platform/crash_trace.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/headless.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/extension.hpp"
#include "src/stormhold/profile.hpp"
#include "src/stormhold/variant.hpp"

namespace {

using StormholdBot = bot::Bot<stormhold::Dungeon>;

int failures = 0;

void check(bool value, const char *message) {
    if (!value) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

const int32_t KEY_DOWN = -2;
const int32_t KEY_SOFT_RIGHT = -7;
const int64_t kTickMs = 250;

const int32_t kBossType = 41;
const int32_t kBossDungeon = 37;

int64_t g_now = 0;
int64_t botClock() { return g_now; }
void botSleep(int64_t ms) {
    if (ms > 0) g_now += ms;
}

int32_t g_pendingJob = 0;
bool g_hasPendingJob = false;

void queueHelperJob(Game *, int32_t job) {
    g_pendingJob = job;
    g_hasPendingJob = true;
}

void drainPendingWork(Game *game) {
    while (g_hasPendingJob) {
        g_hasPendingJob = false;
        game->runHelperJob(g_pendingJob);
    }
}

void primeCanvasLoop(GameCanvas *canvas) { canvas->running_ = true; }
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
    drainPendingWork(game);
    if (game->splashUI_ == nullptr) return false;
    headless::finishSplash(game->splashUI_);

    const int32_t flow[] = {
        KEY_SOFT_RIGHT,
        KEY_SOFT_RIGHT,
        KEY_DOWN,
        KEY_SOFT_RIGHT,
        KEY_SOFT_RIGHT,
    };
    for (int32_t code : flow) {
        press(game, code);
        drainPendingWork(game);
    }
    if (!host::completeNameForm(game->display_, "Bot")) return false;
    drainPendingWork(game);
    for (int n = 0; n < 2; ++n) {
        press(game, KEY_SOFT_RIGHT);
        drainPendingWork(game);
    }
    return game->gameCanvas_ != nullptr && game->character_ != nullptr;
}

int32_t populatedCount(Game *game) {
    int32_t open = 0;
    for (int32_t id = 1; id <= (int32_t)worldstate::DungeonRegistry::kDungeonCount; ++id) {
        if (game->dungeonAt(id)->populated_) ++open;
    }
    return open;
}

}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <resource-dir> --clear|--beat\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[2];
    const bool wantBeat = mode == "--beat";

    crash_trace::install();
    Resources::setRoot(argv[1]);
    const char *tmp = std::getenv("TEST_TMPDIR");
    SaveRecordFiles::setRoot(tmp != nullptr ? std::string(tmp) + "/rms-play"
                                         : std::string("saves/rms-play-sh"));

    render::Surface screen(176, 208);
    render::Context renderer(&screen);
    platform::PlatformContext context;
    context.installFileSystem(platform::defaultContext()->fileSystem());
    context.installSaveStore(platform::defaultContext()->saveStore());
    context.installRenderServices(&renderer);

    g_now = 1000000000000LL;
    context.installClock(botClock);
    context.installSleep(botSleep);

    stormhold::stormhold_init_statics(&context);
    Game game(stormhold::profile(), &context);
    game.setExecutionHooks(queueHelperJob, primeCanvasLoop, noSplashLoop);
    check(createCharacter(&game), "a character was created and reached the dungeon");
    if (failures != 0) return 1;

    StormholdBot bot(&game, &g_now, kTickMs);
    Player *player = bot.player();

    std::printf("at the start: %d gift points, %d dungeons open\n",
                (int)player->giftPoints_, populatedCount(&game));

    const std::vector<bot::SweepResult> swept = bot.sweepAllDungeons();
    int32_t tiles = 0, kills = 0, chests = 0;
    for (const bot::SweepResult &s : swept) {
        tiles += s.tilesWalked;
        kills += s.monstersKilled;
        chests += s.chestsOpened;
    }
    std::printf("swept %d dungeons: %d tiles, %d kills, %d chests, %d levels\n",
                (int)swept.size(), tiles, kills, chests, bot.levelUpsTaken());
    std::printf("after the sweep: %d gift points, %d dungeons open, %d widgets\n",
                (int)player->giftPoints_, populatedCount(&game),
                (int)game.ownedUiWidgets_.size());

    check(swept.size() > 1, "the walk crossed between dungeons");
    check(tiles > 500, "the walk covered real ground");
    check(kills > 0, "the sweep fought something");
    check(chests > 0, "the sweep looted as it went");

    check(player->giftPoints_ > 0, "the sweep gathered gift points");

    if (!wantBeat) {
        if (failures == 0) std::printf("%s: OK\n", mode.c_str());
        return failures == 0 ? 0 : 1;
    }

    stormhold::Dungeon *boss_dungeon =
        static_cast<stormhold::Dungeon *>(game.dungeonAt(kBossDungeon));
    std::printf("dungeon %d populated: %d\n", kBossDungeon,
                (int)boss_dungeon->populated_);
    check(boss_dungeon->populated_,
          "the last dungeon group opened, so the ending is reachable");
    if (!boss_dungeon->populated_) {
        std::printf("gift points reached %d; %d are needed to open the group\n",
                    (int)player->giftPoints_, 48);
        return 1;
    }

    Monster decoded;
    Monster *boss = nullptr;
    for (const worldstate::MonsterRecord &record :
         game.worldState().monsters.at((std::size_t)(kBossDungeon - 1))) {
        Monster *m = Monster::fromRecord(&decoded, record, boss_dungeon);
        if (m != nullptr && m->type_ == kBossType) {
            boss = m;
            break;
        }
    }
    check(boss != nullptr, "the last dungeon holds the boss");
    if (boss == nullptr) return 1;

    const int32_t bossX = boss->gridX_;
    const int32_t bossY = boss->gridY_;

    if (player->dungeonId_ != kBossDungeon) {
        bot.sweepAllDungeons();
        std::printf("after the second pass: in dungeon %d\n", (int)player->dungeonId_);
    }
    check(player->dungeonId_ == kBossDungeon, "the bot reached the last dungeon");

    bot.walkTo(bossX, bossY);
    for (const int32_t facing : {1, 2, 3, 4}) {
        player->facing_ = (int8_t)facing;
        player->refreshSurroundings();
        if (bot.fightAhead(2000)) break;
    }

    for (const bot::Step &s : bot::kSteps) {
        if (!bot.walkable(bossX - s.dx, bossY - s.dy)) continue;
        if (!bot.walkTo(bossX - s.dx, bossY - s.dy)) continue;
        player->facing_ = (int8_t)s.facing;
        player->refreshSurroundings();
        break;
    }

    game.gameCanvas_->pendingMove_ = 1;
    g_now += kTickMs;
    game.gameCanvas_->repaintEnabled_ = false;
    game.gameCanvas_->tick();

    check(player->gameWon_, "picking up what the boss dropped won the game");
    check(screenOf(&game) == uistate::SCREEN_END_OF_GAME,
          "the Victory screen is up");

    if (failures == 0) std::printf("%s: OK\n", mode.c_str());
    return failures == 0 ? 0 : 1;
}
