#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "src/common/host/registry.hpp"
#include "src/common/ui.hpp"
#include "src/common/platform/crash_trace.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/platform/intake.hpp"
#include "src/common/platform/platform.hpp"
#include "src/common/platform/port_settings.hpp"
#include "src/common/platform/sdl_audio.hpp"
#include "src/common/platform/version.hpp"
#include "src/common/render/render.hpp"
#include "src/common/render/widescreen.hpp"

namespace {

const int kScreenWidth = widescreen::kWideWidth;
const int kScreenHeight = widescreen::kHeight;
const int kNarrowWidth = widescreen::kNarrowWidth;

const int64_t kRepeatPeriodMs = 250;

void discardSdlLog(void *, int, SDL_LogPriority, const char *) {}

enum : int32_t {
    KEY_UP = -1,
    KEY_DOWN = -2,
    KEY_LEFT = -3,
    KEY_RIGHT = -4,
    KEY_SOFT_LEFT = -6,
    KEY_SOFT_RIGHT = -7,
    KEY_STAR = 42,
    KEY_STRAFE_LEFT = 52,
    KEY_STRAFE_RIGHT = 54,
};

shortcuts::Binding g_shortcutBindings[8];
int g_shortcutCount = 0;

void buildShortcutBindings(host::GameHost *game) {
    struct Wanted {
        SDL_Keycode key;
        menuaction::Action action;
        const char *name;
    };
    const Wanted wanted[] = {
        {SDLK_TAB, menuaction::STATS, "stats"},
        {SDLK_I, menuaction::INVENTORY, "inventory"},
        {SDLK_J, menuaction::CLUE_LOG, "journal"},
        {SDLK_K, menuaction::SKILLS, "skills"},
        {SDLK_B, menuaction::SPELLS, "spells"},
        {SDLK_H, menuaction::HELP, "help"},
        {SDLK_F6, menuaction::SAVE_GAME, "quicksave"},
        {SDLK_F9, menuaction::LOAD_GAME, "quickload"},
    };
    int32_t rowCount = 0;
    const menuaction::Action *rows = game->optionsRows(&rowCount);
    g_shortcutCount = 0;
    for (const Wanted &w : wanted) {
        int32_t row = menuaction::rowOf(rows, rowCount, w.action);
        if (row < 0) continue;
        g_shortcutBindings[g_shortcutCount].key = w.key;
        g_shortcutBindings[g_shortcutCount].row = (int)row;
        g_shortcutBindings[g_shortcutCount].label = w.name;
        ++g_shortcutCount;
    }
}

bool isMoveKey(int32_t code) {
    return code == KEY_UP || code == KEY_DOWN || code == KEY_STRAFE_LEFT ||
           code == KEY_STRAFE_RIGHT;
}

int32_t mapKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:
        case SDLK_W: return KEY_UP;
        case SDLK_DOWN:
        case SDLK_S: return KEY_DOWN;
        case SDLK_LEFT:
        case SDLK_A: return KEY_LEFT;
        case SDLK_RIGHT:
        case SDLK_D: return KEY_RIGHT;

        case SDLK_Q: return KEY_STRAFE_LEFT;
        case SDLK_E: return KEY_STRAFE_RIGHT;

        // Menu-only list jumps.  The game canvas ignores these codes.
        case SDLK_HOME: return shortcuts::JUMP_HOME;
        case SDLK_END: return shortcuts::JUMP_END;
        case SDLK_PAGEUP: return shortcuts::JUMP_PAGE_UP;
        case SDLK_PAGEDOWN: return shortcuts::JUMP_PAGE_DOWN;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: return KEY_SOFT_RIGHT;
        case SDLK_BACKSPACE: return KEY_SOFT_LEFT;

        case SDLK_F: return 49;
        case SDLK_C: return 51;
        case SDLK_R: return 53;
        case SDLK_T: return 57;
        case SDLK_Z: return 48;
        case SDLK_O: return 55;
        case SDLK_M:
        case SDLK_ASTERISK:
        case SDLK_KP_MULTIPLY: return KEY_STAR;

        case SDLK_0: return 48;
        case SDLK_1: return 49;
        case SDLK_2: return 50;
        case SDLK_3: return 51;
        case SDLK_4: return 52;
        case SDLK_5: return 53;
        case SDLK_6: return 54;
        case SDLK_7: return 55;
        case SDLK_8: return 56;
        case SDLK_9: return 57;

        case SDLK_KP_7: return 49;
        case SDLK_KP_8: return 50;
        case SDLK_KP_9: return 51;
        case SDLK_KP_4: return 52;
        case SDLK_KP_5: return 53;
        case SDLK_KP_6: return 54;
        case SDLK_KP_1: return 55;
        case SDLK_KP_2: return 56;
        case SDLK_KP_3: return 57;
        case SDLK_KP_0: return 48;
        default: return 0;
    }
}

void applyPresentation(SDL_Renderer *renderer, SDL_Window *window, bool wide,
                       bool fullscreen, int scale) {
    int32_t logical = wide ? kScreenWidth : kNarrowWidth;
    SDL_SetRenderLogicalPresentation(renderer, logical, kScreenHeight,
                                     fullscreen
                                         ? SDL_LOGICAL_PRESENTATION_LETTERBOX
                                         : SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    if (!fullscreen) {
        SDL_SetWindowSize(window, logical * scale, kScreenHeight * scale);
    }
}

void leaveFullscreenBeforeDestroy(SDL_Window *window, bool fullscreen) {
    if (!fullscreen) return;
    // Finish the asynchronous fullscreen transition before application shutdown.
    SDL_SetWindowFullscreen(window, false);
    SDL_SyncWindow(window);
}

struct Presentation {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *frame = nullptr;
    bool fullscreen = false;

    bool open(int scale) {
        if (frame != nullptr) return true;
        window = SDL_CreateWindow("Raingrasp", kNarrowWidth * scale,
                                  kScreenHeight * scale, SDL_WINDOW_RESIZABLE);
        // Let SDL pick the renderer.  The software path was tried against an
        // occasional crash that turned out to be a leaked frame lock, and it
        // costs a GPU-backed surface -- which is what screen capture tools
        // hook, so software rendering leaves them with nothing to grab.
        if (window != nullptr) {
            renderer = SDL_CreateRenderer(window, nullptr);
            if (renderer == nullptr) {
                // Keep the software path as a fallback: on a machine where the
                // accelerated device really does fail, starting without capture
                // beats not starting.
                platform::writeLogLine(std::string("accelerated renderer unavailable (") +
                                       SDL_GetError() + "), falling back to software");
                renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
            }
        }
        if (renderer != nullptr) {
            const char *name = SDL_GetRendererName(renderer);
            platform::writeLogLine(std::string("renderer: ") +
                                   (name != nullptr ? name : "(unknown)"));
            frame = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      kScreenWidth, kScreenHeight);
        }
        if (frame == nullptr) {
            close();
            return false;
        }
        SDL_SetTextureScaleMode(frame, SDL_SCALEMODE_NEAREST);
        return true;
    }

    void close() {
        if (window != nullptr) leaveFullscreenBeforeDestroy(window, fullscreen);
        SDL_DestroyTexture(frame);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        frame = nullptr;
        renderer = nullptr;
        window = nullptr;
        fullscreen = false;
    }
};

Form *currentForm(Display *display) {
    return dynamic_cast<Form *>(display->getCurrent());
}
Alert *currentAlert(Display *display) {
    return dynamic_cast<Alert *>(display->getCurrent());
}

TextField *formTextField(Form *form) {
    if (form == nullptr) return nullptr;
    for (int32_t n = 0; n < form->size(); ++n) {
        if (TextField *field = dynamic_cast<TextField *>(form->get(n))) {
            return field;
        }
    }
    return nullptr;
}

std::vector<std::string> wrapText(const std::string &text, size_t columns) {
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    for (size_t n = 0; n <= text.size(); ++n) {
        char ch = n < text.size() ? text[n] : ' ';
        if (ch == ' ' || ch == '\n') {
            if (!word.empty()) {
                if (line.empty()) {
                    line = word;
                } else if (line.size() + 1 + word.size() <= columns) {
                    line += " " + word;
                } else {
                    lines.push_back(line);
                    line = word;
                }
                word.clear();
            }
            if (ch == '\n') {
                lines.push_back(line);
                line.clear();
            }
        } else {
            word += ch;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void drawOverlay(render::Surface *screen, const std::string &title,
                 const std::vector<std::string> &body, bool showInput,
                 const std::string &input, const std::string &hint) {
    render::SoftGraphics graphics(screen);
    graphics.setOrigin(widescreen::originX(), 0);
    graphics.setColor(0);
    graphics.fillRect(-widescreen::originX(), 0, kScreenWidth, kScreenHeight);
    graphics.setColor(2510210);
    graphics.fillRect(0, 0, kNarrowWidth, kScreenHeight);
    graphics.setColor(0);
    graphics.fillRect(0, 0, kNarrowWidth, 14);
    graphics.setColor(0xFFFFFF);
    graphics.setFont(Font::getFont(0, 1, 0));
    graphics.drawString(title, kNarrowWidth / 2, 1, render::HCENTER | render::TOP);

    graphics.setFont(Font::getFont(0, 1, 8));
    int32_t y = 24;
    graphics.setColor(0xFFFF00);
    for (const std::string &line : body) {
        graphics.drawString(std::string(line), 8, y, render::LEFT | render::TOP);
        y += 13;
    }

    if (showInput) {
        y += 6;
        graphics.setColor(0xFFFFFF);
        graphics.fillRect(8, y, kNarrowWidth - 16, 16);
        graphics.setColor(0);
        graphics.drawString(std::string(input + "_"), 12, y + 2, render::LEFT | render::TOP);
    }

    graphics.setColor(0xFFFFFF);
    graphics.drawString(std::string(hint), kNarrowWidth / 2, kScreenHeight - 16,
                        render::HCENTER | render::TOP);
}

std::string lastPlayedPath() {
    return platform::intake::settingsDir() + "/last-played";
}

platform::intake::Variant loadLastPlayed() {
    std::FILE *f = std::fopen(lastPlayedPath().c_str(), "rb");
    if (f == nullptr) return platform::intake::Variant::Unknown;
    char buf[64] = {0};
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
        buf[--n] = 0;
    }
    return platform::intake::variantFromId(buf);
}

void saveLastPlayed(platform::intake::Variant v) {
    std::error_code ec;
    std::filesystem::create_directories(platform::intake::settingsDir(), ec);
    std::FILE *f = std::fopen(lastPlayedPath().c_str(), "wb");
    if (f == nullptr) return;
    const char *id = platform::intake::variantId(v);
    std::fwrite(id, 1, std::strlen(id), f);
    std::fclose(f);
}

struct GameChoice {
    platform::intake::Variant variant;
    std::string label;
    std::string tree;
    render::Surface logo;
};

std::vector<GameChoice> installedGames() {
    std::vector<GameChoice> out;
    const platform::intake::Variant both[] = {platform::intake::Variant::Dawnstar,
                                              platform::intake::Variant::Stormhold};
    for (size_t i = 0; i < 2; ++i) {
        platform::intake::Variant v = both[i];
        std::string tree = platform::intake::treeDir(v);
        std::string why;
        if (!platform::intake::treeIsUsable(tree, v, &why)) continue;
        GameChoice choice;
        choice.variant = v;
        choice.tree = tree;
        choice.label = platform::intake::variantId(v);
        if (!choice.label.empty()) {
            choice.label[0] = (char)(choice.label[0] - 'a' + 'A');
        }
        render::readPng(tree + "/splashbot.png", &choice.logo);
        out.push_back(choice);
    }
    return out;
}

void paintGameSelect(render::Surface *screen, const std::vector<GameChoice> &games,
                     size_t selected, bool cancellable) {
    render::SoftGraphics graphics(screen);
    graphics.setOrigin(0, 0);
    graphics.setColor(0);
    graphics.fillRect(0, 0, kScreenWidth, kScreenHeight);

    graphics.setColor(0xFFFFFF);
    graphics.setFont(Font::getFont(0, 1, 0));
    graphics.drawString(std::string("Select a Game"), kNarrowWidth / 2, 10,
                        render::HCENTER | render::TOP);

    const int32_t kRow = 56;
    const int32_t kTop = 40;
    for (size_t n = 0; n < games.size(); ++n) {
        int32_t y = kTop + (int32_t)n * kRow;
        if (n == selected) {
            graphics.setColor(0x404040);
            graphics.fillRect(4, y, kNarrowWidth - 8, kRow - 6);
        }
        const render::Surface &logo = games[n].logo;
        if (!logo.empty()) {
            int32_t x = (kNarrowWidth - logo.width) / 2;
            int32_t iy = y + (kRow - 6 - logo.height) / 2;
            const uint32_t key = logo.pixels[0] & 0x00FFFFFFu;
            for (int32_t ry = 0; ry < logo.height; ++ry) {
                int32_t dy = iy + ry;
                if (dy < 0 || dy >= kScreenHeight) continue;
                const uint32_t *src = logo.row(ry);
                uint32_t *dst = screen->row(dy);
                for (int32_t rx = 0; rx < logo.width; ++rx) {
                    int32_t dx = x + rx;
                    if (dx < 0 || dx >= kScreenWidth) continue;
                    uint32_t px = src[rx];
                    if ((px & 0x00FFFFFFu) == key) continue;
                    if ((px >> 24) < 0x80) continue;
                    dst[dx] = px | 0xFF000000u;
                }
            }
        } else {
            graphics.setColor(0xFFFFFF);
            graphics.setFont(Font::getFont(0, 1, 8));
            graphics.drawString(std::string(games[n].label), kNarrowWidth / 2, y + 18,
                                render::HCENTER | render::TOP);
        }
    }

    graphics.setColor(0xFFFFFF);
    graphics.setFont(Font::getFont(0, 0, 8));
    graphics.drawString(std::string(cancellable ? "Enter - select, Esc - back"
                                           : "Enter - select"),
                        kNarrowWidth / 2, kScreenHeight - 14,
                        render::HCENTER | render::TOP);
}

platform::intake::Variant runGameSelect(SDL_Renderer *renderer, SDL_Window *window,
                                        SDL_Texture *frame, render::Surface *screen,
                                        const std::vector<GameChoice> &games,
                                        size_t selected, bool cancellable,
                                        int scale, bool *fullscreen, bool *closed) {
    *closed = false;
    if (games.empty()) return platform::intake::Variant::Unknown;
    if (selected >= games.size()) selected = 0;
    // Game Select is always a narrow UI, even when it reuses a renderer that
    // was presenting the dungeon in widescreen mode.
    applyPresentation(renderer, window, false, *fullscreen, scale);

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                *closed = true;
                return platform::intake::Variant::Unknown;
            }
            if (event.type != SDL_EVENT_KEY_DOWN) continue;
            SDL_Keycode key = event.key.key;
            if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
                (event.key.mod & SDL_KMOD_ALT) != 0) {
                if (!event.key.repeat) {
                    const bool next = !*fullscreen;
                    if (SDL_SetWindowFullscreen(window, next)) {
                        *fullscreen = next;
                        applyPresentation(renderer, window, false, *fullscreen, scale);
                    }
                }
            } else if (key == SDLK_UP || key == SDLK_W || key == SDLK_KP_8) {
                selected = selected == 0 ? games.size() - 1 : selected - 1;
            } else if (key == SDLK_DOWN || key == SDLK_S || key == SDLK_KP_2) {
                selected = selected + 1 >= games.size() ? 0 : selected + 1;
            } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER ||
                       key == SDLK_SPACE) {
                return games[selected].variant;
            } else if (key == SDLK_ESCAPE && cancellable) {
                return platform::intake::Variant::Unknown;
            }
        }

        paintGameSelect(screen, games, selected, cancellable);
        SDL_UpdateTexture(frame, nullptr, screen->pixels.data(),
                          kScreenWidth * (int)sizeof(uint32_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_FRect src{0.0f, 0.0f, (float)kNarrowWidth, (float)kScreenHeight};
        SDL_RenderTexture(renderer, frame, &src, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

bool runIntakeScreen(platform::intake::Request want, int scale,
                     platform::intake::Result *out) {
    SDL_Window *window = SDL_CreateWindow("Raingrasp", kNarrowWidth * scale,
                                          kScreenHeight * scale, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        return false;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        SDL_DestroyWindow(window);
        return false;
    }
    SDL_SetRenderLogicalPresentation(renderer, kNarrowWidth, kScreenHeight,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    render::Surface screen(kScreenWidth, kScreenHeight);
    SDL_Texture *frame = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING, kScreenWidth,
                                           kScreenHeight);
    SDL_SetTextureScaleMode(frame, SDL_SCALEMODE_NEAREST);

    const std::string invitation =
        "Drop the game's .jar\n"
        "on this window.";
    std::string message = invitation;
    bool resolved = false;
    bool closed = false;

    while (!resolved && !closed) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                closed = true;
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.key == SDLK_ESCAPE) {
                closed = true;
                continue;
            }
            if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr) {
                std::string dropped = event.drop.data;
                platform::intake::Variant which =
                    platform::intake::identifyJar(dropped);
                if (which == platform::intake::Variant::Unknown) {
                    message = "That file is not a Dawnstar or Stormhold JAR.\n\n"
                              "Drop the game's own .jar here.";
                    continue;
                }
                std::string error;
                if (!platform::intake::unpackJar(
                        dropped, platform::intake::treeDir(which), which, &error)) {
                    message = error;
                    continue;
                }
                platform::intake::Request now = want;
                now.jarPath.clear();
                now.bareArg.clear();
                if (now.want == platform::intake::Variant::Unknown) now.want = which;
                platform::intake::Result again = platform::intake::resolve(now);
                if (again.status == platform::intake::Result::Status::Ready) {
                    *out = again;
                    resolved = true;
                } else {
                    message = again.message;
                }
                continue;
            }
        }
        if (resolved || closed) break;

        {
            render::SoftGraphics graphics(&screen);
            graphics.setOrigin(0, 0);
            graphics.setColor(0);
            graphics.fillRect(0, 0, kScreenWidth, kScreenHeight);
            graphics.setColor(0xFFFFFF);
            graphics.setFont(Font::getFont(0, 1, 0));
            graphics.drawString(std::string("Raingrasp"), kNarrowWidth / 2, 8,
                                render::HCENTER | render::TOP);
            graphics.setFont(Font::getFont(0, 0, 8));
            const int32_t kLine = 12;
            const int32_t kTop = 34;
            const int32_t kBottom = kScreenHeight - 24;
            std::vector<std::string> lines = wrapText(message, 28);
            for (size_t n = 0; n < lines.size(); ++n) {
                if (lines[n].size() <= 28) continue;
                std::string rest = lines[n].substr(28);
                lines[n].resize(28);
                lines.insert(lines.begin() + (ptrdiff_t)n + 1, rest);
            }
            size_t room = (size_t)((kBottom - kTop) / kLine);
            if (lines.size() > room) lines.resize(room);
            int32_t block = (int32_t)lines.size() * kLine;
            int32_t y = kTop + ((kBottom - kTop) - block) / 2;
            if (y < kTop) y = kTop;
            for (size_t n = 0; n < lines.size(); ++n) {
                graphics.drawString(std::string(lines[n]), kNarrowWidth / 2, y,
                                    render::HCENTER | render::TOP);
                y += kLine;
            }
            graphics.drawString(std::string("Esc to quit"), kNarrowWidth / 2,
                                kScreenHeight - 14, render::HCENTER | render::TOP);
            SDL_UpdateTexture(frame, nullptr, screen.pixels.data(),
                              kScreenWidth * (int)sizeof(uint32_t));
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_FRect src{0.0f, 0.0f, (float)kNarrowWidth, (float)kScreenHeight};
        SDL_RenderTexture(renderer, frame, &src, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(frame);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return resolved;
}

}

int main(int argc, char **argv) {
    platform::setLoggingEnabled(false);
    SDL_SetLogOutputFunction(discardSdlLog, nullptr);

    if (argc >= 2 && (std::strcmp(argv[1], "--help") == 0 ||
                      std::strcmp(argv[1], "-h") == 0)) {
        return 0;
    }

    std::string saveDir;
    std::string soundDir;
    int scale = 3;
    bool portable = false;
    platform::intake::Request want;
    {
        bool tookBare = false;
        for (int n = 1; n < argc; ++n) {
            if (std::strcmp(argv[n], "--scale") == 0 && n + 1 < argc) {
                scale = std::atoi(argv[++n]);
                if (scale < 1) scale = 1;
            } else if (std::strcmp(argv[n], "--game") == 0 && n + 1 < argc) {
                want.want = platform::intake::variantFromId(argv[++n]);
                if (want.want == platform::intake::Variant::Unknown) {
                    return 2;
                }
            } else if (std::strcmp(argv[n], "--jar") == 0 && n + 1 < argc) {
                want.jarPath = argv[++n];
            } else if (std::strcmp(argv[n], "--data") == 0 && n + 1 < argc) {
                want.dataDir = argv[++n];
            } else if (std::strcmp(argv[n], "--sounds") == 0 && n + 1 < argc) {
                soundDir = argv[++n];
            } else if (std::strcmp(argv[n], "--portable") == 0) {
                portable = true;
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
        want.exeDir =
            slash == std::string::npos ? std::string(".") : exe.substr(0, slash);
        platform::intake::configurePortableMode(want.exeDir, portable);
    }

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }

    // Open the log before anything that can fail or draw, so the crash handler
    // and the renderer choice are both covered even when the game-select
    // screen runs first.
    {
        const std::string logDir = platform::intake::settingsDir();
        std::error_code logDirError;
        std::filesystem::create_directories(logDir, logDirError);
        platform::setLogFile(logDir + "/raingrasp.log");
        crash_trace::install();
#if defined(RAINGRASP_THROW_TRACE)
        // Debug build only (//src/common/sdl:raingrasp_debug).  The game
        // catches its own exceptions, so without this a bounds throw reaches
        // the log as a bare message with the guilty frames already unwound.
        crash_trace::installThrowTrace();
        platform::writeLogLine("throw-site tracing enabled");
#endif
        platform::writeLogLine("=== Raingrasp session, " +
                               platform::version::describe() + " ===");
    }

    std::optional<bool> selectedFullscreen;
    Presentation presentation;

    bool askedForGame = want.want != platform::intake::Variant::Unknown;
    if (!askedForGame) {
        platform::intake::Variant remembered = loadLastPlayed();
        if (remembered != platform::intake::Variant::Unknown) {
            std::string why;
            if (platform::intake::treeIsUsable(platform::intake::treeDir(remembered),
                                               remembered, &why)) {
                want.want = remembered;
            }
        }
    }

    if (!askedForGame && want.want == platform::intake::Variant::Unknown) {
        std::vector<GameChoice> games = installedGames();
        if (games.size() > 1) {
            if (!presentation.open(scale)) {
                SDL_Quit();
                return 1;
            }
            render::Surface ps(kScreenWidth, kScreenHeight);
            bool closed = false;
            want.want = runGameSelect(presentation.renderer, presentation.window,
                                      presentation.frame, &ps, games, 0, false, scale,
                                      &presentation.fullscreen, &closed);
            selectedFullscreen = presentation.fullscreen;
            if (closed) {
                presentation.close();
                SDL_Quit();
                return 0;
            }
        }
    }

    platform::intake::Result data = platform::intake::resolve(want);
    if (data.status != platform::intake::Result::Status::Ready) {
        if (!runIntakeScreen(want, scale, &data)) {
            presentation.close();
            SDL_Quit();
            return 2;
        }
    }
    const std::string requestedSaveDir = saveDir;
    const std::string settingsDir = platform::intake::settingsDir();
    // Presentation belongs to the application, not an individual game. In
    // particular, replacing a fullscreen Direct3D device between sessions can
    // fail inside DXCore on Windows. Reboot only the game and its services.
    if (!presentation.open(scale)) {
        SDL_Quit();
        return 1;
    }
    SDL_Window *window = presentation.window;
    SDL_Renderer *renderer = presentation.renderer;
    SDL_Texture *frame = presentation.frame;
    bool &fullscreen = presentation.fullscreen;
    for (;;) {
    std::string resourceDir = data.tree;
    saveDir = requestedSaveDir;
    saveLastPlayed(data.variant);
    if (saveDir.empty()) saveDir = platform::intake::saveDir(data.variant);
    auto resourceService =
        std::make_unique<platform::desktop::DesktopResourceStack>(resourceDir);
    auto saveService = std::make_unique<platform::DirectorySaveStore>(saveDir);
    render::Surface screen(kScreenWidth, kScreenHeight);
    render::Context renderContext(&screen);
    platform::PlatformContext context;
    context.installFileSystem(resourceService.get());
    context.installSaveStore(saveService.get());
    context.installRenderServices(&renderContext);
    platform::SdlAudioSink audioSink;
    const std::string sessionSoundDir = soundDir.empty() ? resourceDir + "/sounds" : soundDir;
    if (audioSink.open(sessionSoundDir)) {
        context.installAudio(&audioSink);
    }
    // Declared last so the host dies before the services it borrows.
    std::unique_ptr<host::GameHost> game =
        host::createHost(platform::intake::variantId(data.variant));
    if (game == nullptr) break;
    bool applyOptions = false;
    int32_t heldMoveKey = 0;
    int64_t nextRepeatMs = 0;
    buildShortcutBindings(game.get());
    shortcuts::install(game->shortcutHost(), g_shortcutBindings, g_shortcutCount);

    platform::loadPortOptions(settingsDir, &context.portOptions());
    if (selectedFullscreen.has_value()) {
        context.portOptions().fullscreen = *selectedFullscreen;
        selectedFullscreen.reset();
    }
    struct OptionsSink {
        std::string dir;
        platform::PortOptions *options;
        host::GameHost *game;
        bool *apply;
    } optionsSink{settingsDir, &context.portOptions(), game.get(), &applyOptions};
    context.installPortOptionsListener(
        [](void *ctx) {
            OptionsSink *sink = (OptionsSink *)ctx;
            platform::savePortOptions(sink->dir, *sink->options);
            sink->game->refreshPortOptionsUI();
            *sink->apply = true;
        },
        &optionsSink);

    SDL_SetWindowTitle(window, game->title());
    applyPresentation(renderer, window, false, fullscreen, scale);

    game->boot(&context, &renderContext, &screen);

    bool running = true;
    bool switching = false;
    bool textInputActive = false;
    Form *lastForm = nullptr;
    int64_t nextTickMs = (int64_t)SDL_GetTicks();
    int64_t nextSplashMs = (int64_t)SDL_GetTicks();

    while (running) {
        Display *display = game->display();
        Form *form = currentForm(display);
        Alert *alert = currentAlert(display);
        TextField *field = formTextField(form);
        if (form != nullptr) lastForm = form;

        bool wantText = field != nullptr;
        if (wantText != textInputActive) {
            if (wantText) {
                SDL_StartTextInput(window);
            } else {
                SDL_StopTextInput(window);
            }
            textInputActive = wantText;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                continue;
            }
            if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr) {
                std::string dropped = event.drop.data;
                platform::intake::Variant which =
                    platform::intake::identifyJar(dropped);
                if (which == platform::intake::Variant::Unknown) {
                    continue;
                }
                std::string error;
                platform::intake::unpackJar(
                    dropped, platform::intake::treeDir(which), which, &error);
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE &&
                !event.key.repeat) {
                if (shortcuts::onBack()) continue;
                shortcuts::cancel();
                const shortcuts::Host *ui = game->shortcutHost();
                ui->pressKey(ui->inGame() ? 55 : KEY_SOFT_LEFT);
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) &&
                (event.key.mod & SDL_KMOD_ALT) != 0) {
                context.portOptions().fullscreen = !context.portOptions().fullscreen;
                context.notifyPortOptionsChanged();
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F11 &&
                !event.key.repeat) {
                context.portOptions().widescreen = !context.portOptions().widescreen;
                context.notifyPortOptionsChanged();
                continue;
            }

            if (alert != nullptr) {
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE ||
                     event.key.key == SDLK_KP_ENTER)) {
                    if (lastForm != nullptr) display->setCurrent(lastForm);
                }
                continue;
            }

            if (form != nullptr) {
                if (event.type == SDL_EVENT_TEXT_INPUT && field != nullptr) {
                    std::string current = field->getString();
                    for (const char *p = event.text.text; *p != '\0'; ++p) {
                        unsigned char ch = (unsigned char)*p;
                        if (ch >= 0x20 && ch < 0x7F && current.size() < 10) {
                            current += (char)ch;
                        }
                    }
                    field->setString(std::string(current));
                } else if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_BACKSPACE && field != nullptr) {
                        std::string current = field->getString();
                        if (!current.empty()) {
                            current.erase(current.size() - 1);
                            field->setString(std::string(current));
                        }
                    } else if (event.key.key == SDLK_RETURN ||
                               event.key.key == SDLK_KP_ENTER) {
                        const std::vector<Command *> &commands = form->commands();
                        if (form->listener() != nullptr && !commands.empty()) {
                            form->listener()->commandAction(commands[0], form);
                        }
                    }
                }
                continue;
            }

            // Confirm cuts the intro logos short.  This sits ahead of the
            // canvas gate because there is no canvas yet while the splash is
            // up.  hasSplash() only says the widget exists -- it stays true for
            // the whole session -- so the key is consumed only when skipSplash
            // reports it actually acted, otherwise Enter would never reach a
            // menu again.
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                game->hasSplash() &&
                (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER ||
                 event.key.key == SDLK_SPACE) &&
                game->skipSplash()) {
                continue;
            }

            if (!game->hasCanvas()) {
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_BACKSPACE && shortcuts::onBack()) continue;
                if (shortcuts::onKey((int32_t)event.key.key)) continue;
                int32_t code = mapKey(event.key.key);
                if (code != 0) {
                    game->keyPressed(code);
                    if (isMoveKey(code)) {
                        heldMoveKey = code;
                        nextRepeatMs = (int64_t)SDL_GetTicks() + kRepeatPeriodMs;
                    }
                }
            } else if (event.type == SDL_EVENT_KEY_UP) {
                int32_t code = mapKey(event.key.key);
                if (code != 0) {
                    game->keyReleased(code);
                    if (code == heldMoveKey) heldMoveKey = 0;
                }
            }
        }

        {
            const platform::PortOptions &options = context.portOptions();
            if (applyOptions || options.widescreen != game->wideView() ||
                options.fullscreen != fullscreen) {
                applyOptions = false;
                const bool wide = options.widescreen;
                game->setWideView(wide);
                if (options.fullscreen != fullscreen) {
                    fullscreen = options.fullscreen;
                    SDL_SetWindowFullscreen(window, fullscreen);
                }
                applyPresentation(renderer, window, wide, fullscreen, scale);
                game->repaintCanvasNow();
            }
        }

        if (heldMoveKey != 0 && context.portOptions().moveAutorepeat &&
            game->canvasRunning() && (int64_t)SDL_GetTicks() >= nextRepeatMs) {
            game->keyPressed(heldMoveKey);
            nextRepeatMs = (int64_t)SDL_GetTicks() + kRepeatPeriodMs;
        }

        host::pump(game.get(), (int64_t)SDL_GetTicks(), &nextTickMs, &nextSplashMs);

        if (game->switchRequested()) {
            switching = true;
            running = false;
        }

        shortcuts::tick();

        form = currentForm(display);
        alert = currentAlert(display);
        field = formTextField(form);
        if (form != nullptr) lastForm = form;

        {
            std::lock_guard<std::mutex> guard(renderContext.frameMutex());
            if (alert != nullptr) {
                drawOverlay(&screen, alert->title(),
                            wrapText(alert->message(), 30), false, "",
                            "Enter to continue");
            } else if (form != nullptr) {
                std::string body;
                for (int32_t n = 0; n < form->size(); ++n) {
                    if (StringItem *item = dynamic_cast<StringItem *>(form->get(n))) {
                        body += item->text;
                    }
                }
                std::string typed = field != nullptr ? field->getString() : "";
                drawOverlay(&screen, form->title(), wrapText(body, 30),
                            field != nullptr, typed,
                            field != nullptr ? "Type a name, Enter = OK" : "Enter = OK");
            }
            SDL_UpdateTexture(frame, nullptr, screen.pixels.data(),
                              kScreenWidth * (int)sizeof(uint32_t));
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (game->wideView()) {
            SDL_RenderTexture(renderer, frame, nullptr, nullptr);
        } else {
            SDL_FRect src = {(float)widescreen::originX(), 0.0f, (float)kNarrowWidth,
                             (float)kScreenHeight};
            SDL_RenderTexture(renderer, frame, &src, nullptr);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(4);
    }

    if (switching) {
        std::vector<GameChoice> games = installedGames();
        size_t current = 0;
        for (size_t n = 0; n < games.size(); ++n) {
            if (games[n].variant == data.variant) current = n;
        }
        bool closed = false;
        platform::intake::Variant picked =
            runGameSelect(renderer, window, frame, &screen, games, current, true,
                          scale, &fullscreen, &closed);
        selectedFullscreen = fullscreen;
        shortcuts::install(nullptr, nullptr, 0);
        if (textInputActive) SDL_StopTextInput(window);
        if (closed) break;
        if (picked == platform::intake::Variant::Unknown) picked = data.variant;
        platform::intake::Request again;
        again.want = picked;
        again.exeDir = want.exeDir;
        platform::intake::Result next = platform::intake::resolve(again);
        if (next.status != platform::intake::Result::Status::Ready) {
            break;
        }
        data = next;
        continue;
    }

    shortcuts::install(nullptr, nullptr, 0);
    break;
    }

    presentation.close();
    SDL_Quit();
    std::_Exit(0);
}
