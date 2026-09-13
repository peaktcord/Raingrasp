#ifndef COMMON_GAME_UISTATE_HPP
#define COMMON_GAME_UISTATE_HPP

#include "src/common/runtime.hpp"

#include <string>

namespace uistate {

constexpr int32_t LAYOUT_DOWNLOAD              = 1;
constexpr int32_t LAYOUT_SPLASH                = 2;
constexpr int32_t LAYOUT_LIST                  = 3;
constexpr int32_t LAYOUT_TEXTBOX               = 4;
constexpr int32_t LAYOUT_FORM_1                = 5;
constexpr int32_t LAYOUT_FORM_2                = 6;
constexpr int32_t LAYOUT_PROGRESS_NEW_GAME     = 8;
constexpr int32_t LAYOUT_PROGRESS_LOAD_GAME    = 9;
constexpr int32_t LAYOUT_PROGRESS_SAVE_GAME    = 10;
constexpr int32_t LAYOUT_PROGRESS_LOAD_DUNGEON = 11;

constexpr int32_t SCREEN_SPLASH = 1;
constexpr int32_t SCREEN_MAIN_MENU = 2;
constexpr int32_t SCREEN_CLASS_SELECT = 3;
constexpr int32_t SCREEN_CHARACTER_SHEET = 4;
constexpr int32_t SCREEN_CLASS_INFO = 5;
constexpr int32_t SCREEN_CHARACTER_CREATED = 6;
constexpr int32_t SCREEN_WELCOME = 7;
constexpr int32_t SCREEN_NPC_GREETING = 8;
constexpr int32_t SCREEN_NPC_TRAIN_WHAT = 20;
constexpr int32_t SCREEN_NPC_TRAIN_RESPONSE = 21;
constexpr int32_t SCREEN_NPC_GIVE_WHAT = 22;
constexpr int32_t SCREEN_NPC_GIVE_RESPONSE = 23;
constexpr int32_t SCREEN_NPC_BEFRIEND_RESPONSE = 24;
constexpr int32_t SCREEN_NPC_THREATEN_RESPONSE = 25;
constexpr int32_t SCREEN_OPTIONS = 31;
constexpr int32_t SCREEN_STATS = 32;
constexpr int32_t SCREEN_INVENTORY = 33;
constexpr int32_t SCREEN_INVENTORY_ITEM = 34;
constexpr int32_t SCREEN_SKILLS_LIST = 35;
constexpr int32_t SCREEN_SKILL_INFO = 36;
constexpr int32_t SCREEN_SPELLS_LIST = 37;
constexpr int32_t SCREEN_SPELL_INFO = 38;
constexpr int32_t SCREEN_LEVEL_UP = 39;
constexpr int32_t SCREEN_LEVEL_UP_DONE = 40;
constexpr int32_t SCREEN_WARP_RESPONSE = 41;
constexpr int32_t SCREEN_INTRODUCTION = 101;
constexpr int32_t SCREEN_END_OF_GAME = 200;
constexpr int32_t SCREEN_GAME_OVER = 201;
constexpr int32_t SCREEN_CONFIRM_QUIT = 202;
constexpr int32_t SCREEN_HELP = 203;
constexpr int32_t SCREEN_CREDITS = 204;
constexpr int32_t SCREEN_HELP_TOPIC = 206;
constexpr int32_t SCREEN_PROGRESS_NEW_GAME = 301;
constexpr int32_t SCREEN_PROGRESS_LOAD_GAME = 302;
constexpr int32_t SCREEN_PROGRESS_SAVE_GAME = 303;
constexpr int32_t SCREEN_PROGRESS_LOAD_DUNGEON = 304;
constexpr int32_t SCREEN_NO_SAVED_GAME = 305;
constexpr int32_t SCREEN_NPC_CURE_RESPONSE = 353;
constexpr int32_t SCREEN_NPC_RECOVERY_RESPONSE = 355;
constexpr int32_t SCREEN_NPC_GREETING_ALTERNATE = 360;
constexpr int32_t SCREEN_EXIT = 399;
constexpr int32_t SCREEN_EXIT_ALTERNATE = 499;

constexpr int32_t SCREEN_PORT_OPTIONS = 9000;

// Screen ids appear in the log whenever the game swaps screens, so give the
// shared ones a readable name.  Variant-specific ids (stormhold::screens,
// dawnstar::screens) overlap numerically and are left as bare numbers.
inline const char *screenName(int32_t screenId) {
    switch (screenId) {
        case SCREEN_SPLASH: return "splash";
        case SCREEN_MAIN_MENU: return "main menu";
        case SCREEN_CLASS_SELECT: return "class select";
        case SCREEN_CHARACTER_SHEET: return "character sheet";
        case SCREEN_CLASS_INFO: return "class info";
        case SCREEN_CHARACTER_CREATED: return "character created";
        case SCREEN_WELCOME: return "welcome";
        case SCREEN_NPC_GREETING: return "npc greeting";
        case SCREEN_NPC_TRAIN_WHAT: return "npc train what";
        case SCREEN_NPC_TRAIN_RESPONSE: return "npc train response";
        case SCREEN_NPC_GIVE_WHAT: return "npc give what";
        case SCREEN_NPC_GIVE_RESPONSE: return "npc give response";
        case SCREEN_NPC_BEFRIEND_RESPONSE: return "npc befriend response";
        case SCREEN_NPC_THREATEN_RESPONSE: return "npc threaten response";
        case SCREEN_OPTIONS: return "options";
        case SCREEN_STATS: return "stats";
        case SCREEN_INVENTORY: return "inventory";
        case SCREEN_INVENTORY_ITEM: return "inventory item";
        case SCREEN_SKILLS_LIST: return "skills list";
        case SCREEN_SKILL_INFO: return "skill info";
        case SCREEN_SPELLS_LIST: return "spells list";
        case SCREEN_SPELL_INFO: return "spell info";
        case SCREEN_LEVEL_UP: return "level up";
        case SCREEN_LEVEL_UP_DONE: return "level up done";
        case SCREEN_WARP_RESPONSE: return "warp response";
        case SCREEN_INTRODUCTION: return "introduction";
        case SCREEN_END_OF_GAME: return "end of game";
        case SCREEN_GAME_OVER: return "game over";
        case SCREEN_CONFIRM_QUIT: return "confirm quit";
        case SCREEN_HELP: return "help";
        case SCREEN_CREDITS: return "credits";
        case SCREEN_HELP_TOPIC: return "help topic";
        case SCREEN_PROGRESS_NEW_GAME: return "progress: new game";
        case SCREEN_PROGRESS_LOAD_GAME: return "progress: load game";
        case SCREEN_PROGRESS_SAVE_GAME: return "progress: save game";
        case SCREEN_PROGRESS_LOAD_DUNGEON: return "progress: load dungeon";
        case SCREEN_NO_SAVED_GAME: return "no saved game";
        case SCREEN_NPC_CURE_RESPONSE: return "npc cure response";
        case SCREEN_NPC_RECOVERY_RESPONSE: return "npc recovery response";
        case SCREEN_NPC_GREETING_ALTERNATE: return "npc greeting (alternate)";
        case SCREEN_EXIT: return "exit";
        case SCREEN_EXIT_ALTERNATE: return "exit (alternate)";
        case SCREEN_PORT_OPTIONS: return "port options";
        default: return nullptr;
    }
}

// "npc greeting (8)" when the id is known, plain "screen 11" when it is not.
inline std::string describeScreen(int32_t screenId) {
    if (const char *name = screenName(screenId)) {
        return std::string(name) + " (" + std::to_string(screenId) + ")";
    }
    return "screen " + std::to_string(screenId);
}

}

#endif
