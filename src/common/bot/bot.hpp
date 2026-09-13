#ifndef COMMON_BOT_BOT_HPP
#define COMMON_BOT_BOT_HPP

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/smallhelpers.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/world_state.hpp"

namespace bot {

struct SweepResult {
    int32_t dungeonId = 0;
    bool entered = false;
    int32_t tilesWalked = 0;
    int32_t monstersKilled = 0;
    int32_t chestsOpened = 0;
    int32_t swings = 0;
};

struct Step {
    int32_t facing;
    int32_t dx;
    int32_t dy;
};
inline const Step kSteps[4] = {{1, 0, -1}, {2, 1, 0}, {3, 0, 1}, {4, -1, 0}};

inline const int32_t kSummonerStatColumn = 11;
inline const int32_t kSummonerEffect = 2;

inline const int32_t kMonsterCeiling = 30;

template <typename DungeonT>
class Bot {
public:
    Bot(Game *game, int64_t *clock, int64_t tickMs)
        : game_(game), clock_(clock), tickMs_(tickMs) {}

    void tick();

    void heal();

    int32_t clearScreens(int32_t guard = 24);

    int32_t levelUpsTaken() const { return levelUpsTaken_; }

    void setFrameHook(void (*hook)(Game &)) { frameHook_ = hook; }

    bool stepTowards(int32_t facing);

    bool walkTo(int32_t x, int32_t y);

    bool fightAhead(int32_t maxSwings = 400);

    bool faceSummoner();

    int32_t monstersHere() const;

    void capMonsters();

    bool takeChestAhead();

    int32_t equipBest();

    int32_t makeRoom();

    SweepResult sweepCurrentDungeon();

    std::vector<SweepResult> sweepAllDungeons();

    Player *player() const { return game_->character_; }
    Game *game() const { return game_; }
    DungeonT *dungeon() const { return static_cast<DungeonT *>(player()->dungeon()); }

    bool walkable(int32_t x, int32_t y) const;

private:
    bool crossEdge(int32_t edge, int32_t facing, int32_t fromDungeon);
    std::vector<int32_t> routeToUnswept(int32_t from, const std::set<int32_t> &done,
                                        const std::set<std::pair<int32_t, int32_t>> &deadEnds);
    void showFrame();
    int32_t populatedDungeons() const;

    Game *game_;
    int64_t *clock_;
    int64_t tickMs_;
    int32_t stayInDungeon_ = 0;
    bool avoidWarps_ = true;
    int32_t levelUpsTaken_ = 0;
    void (*frameHook_)(Game &) = nullptr;
};

}

#include "src/common/bot/bot_impl.hpp"

#endif
