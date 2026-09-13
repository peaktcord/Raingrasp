#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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
#include "src/common/render/widescreen.hpp"
#include "src/common/replay/headless.hpp"
#include "src/dawnstar/bot_traitor.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/profile.hpp"
#include "src/dawnstar/variant.hpp"

namespace {

const int kScreenWidth = widescreen::kWideWidth;
const int kScreenHeight = widescreen::kHeight;
const int kNarrowWidth = widescreen::kNarrowWidth;

const int32_t KEY_DOWN = -2;
const int32_t KEY_SOFT_RIGHT = -7;
const int32_t kBossType = 42;
const int32_t kBossTick = 140;
const int64_t kTickMs = 250;

int64_t g_now = 1000000000000LL;
int64_t botClock() { return g_now; }
void botSleep(int64_t ms) {
    if (ms > 0) g_now += ms;
}

void noCanvasLoop(GameCanvas *) {}
void noSplashLoop(UIWidget *) {}

struct Window {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *frame = nullptr;
    render::Surface *screen = nullptr;
    int delayMs = 0;
    bool quit = false;
    long frames = 0;
    long shotAt = -1;
    std::string shotPath;
    int screenHoldMs = 100;
    bool shotOnScreen = false;

    bool open(render::Surface *surface, int scale, int delay) {
        screen = surface;
        delayMs = delay;
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return false;
        }
        window = SDL_CreateWindow("Dawnstar -- bot playing", kNarrowWidth * scale,
                                  kScreenHeight * scale, SDL_WINDOW_RESIZABLE);
        if (window == nullptr) {
            std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
            return false;
        }
        renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr) {
            std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetRenderLogicalPresentation(renderer, kNarrowWidth, kScreenHeight,
                                         SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        SDL_SetWindowSize(window, kNarrowWidth * scale, kScreenHeight * scale);
        frame = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, kScreenWidth, kScreenHeight);
        return frame != nullptr;
    }

    void present() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) quit = true;
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_ESCAPE) {
                quit = true;
            }
        }
        SDL_UpdateTexture(frame, nullptr, screen->pixels.data(),
                          kScreenWidth * (int)sizeof(uint32_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_FRect src = {0.0f, 0.0f, (float)kNarrowWidth, (float)kScreenHeight};
        SDL_RenderTexture(renderer, frame, &src, nullptr);
        SDL_RenderPresent(renderer);
        if ((++frames == shotAt || shotOnScreen) && !shotPath.empty()) {
            shotOnScreen = false;
            SDL_Surface *shot = SDL_CreateSurfaceFrom(
                kScreenWidth, kScreenHeight, SDL_PIXELFORMAT_ARGB8888,
                screen->pixels.data(), kScreenWidth * (int)sizeof(uint32_t));
            if (shot != nullptr) {
                SDL_SaveBMP(shot, shotPath.c_str());
                SDL_DestroySurface(shot);
                std::printf("wrote %s at frame %ld\n", shotPath.c_str(), frames);
            }
        }
        if (delayMs > 0) SDL_Delay((Uint32)delayMs);
    }

    void hold(int ms) {
        const Uint64 until = SDL_GetTicks() + (Uint64)ms;
        while (!quit && SDL_GetTicks() < until) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) quit = true;
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    quit = true;
                }
            }
            SDL_Delay(4);
        }
    }

    void close() {
        if (frame != nullptr) SDL_DestroyTexture(frame);
        if (renderer != nullptr) SDL_DestroyRenderer(renderer);
        if (window != nullptr) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

Window g_win;

void press(Game *game, int32_t code) {
    if (Canvas *canvas = game->display_ != nullptr
                             ? dynamic_cast<Canvas *>(game->display_->getCurrent())
                             : nullptr) {
        canvas->keyPressed(code);
        canvas->keyReleased(code);
    }
}

bool createCharacter(Game *game) {
    game->startApplication();
    if (game->splashUI_ == nullptr) return false;
    headless::finishSplash(game->splashUI_);
    const int32_t flow[] = {KEY_SOFT_RIGHT, KEY_SOFT_RIGHT, KEY_DOWN, KEY_SOFT_RIGHT,
                            KEY_SOFT_RIGHT};
    for (int32_t code : flow) press(game, code);
    if (!host::completeNameForm(game->display_, "Bot")) return false;
    for (int n = 0; n < 3; ++n) press(game, KEY_SOFT_RIGHT);
    return game->gameCanvas_ != nullptr && game->character_ != nullptr;
}

}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: %s <resource-dir> [--scale N] [--delay MS] "
                     "[--stage clear|deduce|beat]\n",
                     argv[0]);
        return 2;
    }
    int scale = 3;
    int delay = 8;
    std::string stage = "beat";
    for (int n = 2; n < argc; ++n) {
        if (std::strcmp(argv[n], "--scale") == 0 && n + 1 < argc) {
            scale = std::atoi(argv[++n]);
        } else if (std::strcmp(argv[n], "--delay") == 0 && n + 1 < argc) {
            delay = std::atoi(argv[++n]);
        } else if (std::strcmp(argv[n], "--stage") == 0 && n + 1 < argc) {
            stage = argv[++n];
        } else if (std::strcmp(argv[n], "--hold") == 0 && n + 1 < argc) {
            g_win.screenHoldMs = std::atoi(argv[++n]);
        } else if (std::strcmp(argv[n], "--shot") == 0 && n + 2 < argc) {
            g_win.shotPath = argv[++n];
            g_win.shotAt = std::atol(argv[++n]);
        }
    }

    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/rms-bot-viewer");

    render::Surface screen(kScreenWidth, kScreenHeight);
    render::Context renderer(&screen);
    platform::PlatformContext context;
    context.installFileSystem(platform::defaultContext()->fileSystem());
    context.installSaveStore(platform::defaultContext()->saveStore());
    context.installRenderServices(&renderer);
    context.installClock(botClock);
    context.installSleep(botSleep);

    if (!g_win.open(&screen, scale, delay)) return 1;

    dawnstar::dawnstar_init_statics(&context);
    Game game(dawnstar::profile(), &context);
    game.setExecutionHooks(headless::runJobInline<Game>, noCanvasLoop, noSplashLoop);
    if (!createCharacter(&game)) {
        std::fprintf(stderr, "could not create a character\n");
        g_win.close();
        return 1;
    }

    dawnstar::DawnstarBot bot(&game, &g_now, kTickMs);
    bot.setFrameHook([](Game &g) {
        Canvas *canvas = g.display_ != nullptr
                             ? dynamic_cast<Canvas *>(g.display_->getCurrent())
                             : nullptr;
        if (canvas == nullptr) return;
        if (g.gameCanvas_ != nullptr) g.gameCanvas_->repaintEnabled_ = true;
        canvas->repaint();
        canvas->serviceRepaints();
        g_win.present();
        if (g.currentUI_ != nullptr) {
            if (g.currentUI_->screenId_ == uistate::SCREEN_LEVEL_UP &&
                !g_win.shotPath.empty()) {
                g_win.shotOnScreen = true;
            }
            g_win.hold(g_win.screenHoldMs);
        }
    });

    Player *player = bot.player();
    std::printf("watching the bot play (%s). Esc or close the window to stop.\n",
                stage.c_str());

    if (stage == "clear" || stage == "beat") {
        const std::vector<bot::SweepResult> swept = bot.sweepAllDungeons();
        int32_t tiles = 0, kills = 0, chests = 0;
        for (const bot::SweepResult &s : swept) {
            tiles += s.tilesWalked;
            kills += s.monstersKilled;
            chests += s.chestsOpened;
        }
        std::printf("swept %d dungeons: %d tiles, %d kills, %d chests, %d levels\n",
                    (int)swept.size(), tiles, kills, chests, bot.levelUpsTaken());
    }

    if (!g_win.quit && (stage == "deduce" || stage == "beat")) {
        const dawnstar::Deduction found = dawnstar::deduceTraitor(bot);
        std::printf("deduction: %d questions, %d lies, accusing NPC %d (really %d)\n",
                    found.questionsAsked, found.liesHeard, found.accused,
                    (int)dawnstar::ext(player).traitorId_);
        if (found.solved) dawnstar::accuse(bot, found.accused);
    }

    if (!g_win.quit && stage == "beat") {
        Monster *boss = nullptr;
        Monster decoded;
        const std::size_t dungeonIndex = (std::size_t)(player->dungeonId_ - 1);
        for (int tick = 0; tick < kBossTick * 12 && boss == nullptr && !g_win.quit; ++tick) {
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
        if (boss != nullptr) {
            std::printf("the oracle sent the boss at count %d\n",
                        dawnstar::ext(player).oracleIndex_);
            Monster target = *boss;
            bot.walkTo(target.gridX_, target.gridY_);
            bot.fightAhead(2000);
            bot.tick();
        }
        const int32_t screenId =
            game.currentUI_ != nullptr ? game.currentUI_->screenId_ : -1;
        std::printf("%s\n", screenId == uistate::SCREEN_END_OF_GAME
                                ? "Victory."
                                : "did not reach the Victory screen");
    }

    for (int n = 0; n < 250 && !g_win.quit; ++n) g_win.present();
    g_win.close();
    return 0;
}
