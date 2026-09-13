#ifndef COMMON_GAME_EXTENSION_HPP
#define COMMON_GAME_EXTENSION_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/visibility.hpp"

class GameCanvas;
class Graphics;
class Monster;
class Player;

namespace game {

class Extension {
public:
    virtual ~Extension() = default;

    virtual int32_t skillRankBonus(const Player &player) {
        (void)player;
        return 0;
    }

    virtual void onInitFromClass(Player &player) { (void)player; }

    virtual void onNewLife(Player &player) { (void)player; }

    virtual void onPlaceAtStart(Player &player) { (void)player; }
    virtual void onEdgeCandidate(Player &player) { (void)player; }
    virtual void onRest(Player &player) { (void)player; }

    virtual void beforeMove(Player &player) { (void)player; }

    virtual void beforeStep(Player &player) { (void)player; }

    virtual bool takeItemsUnderfoot(Player &player, int8_t tile, bool forward) = 0;

    virtual void onGiftPoints(Player &player) { (void)player; }

    virtual void placeVisibleNpcs(Player &player) = 0;
    virtual bool npcPosition(const Player &player, int32_t kind, const visibility::Slot &object,
                             int8_t &x, int8_t &y) = 0;
    virtual int32_t npcAhead(const Player &player, int32_t dungeonId, int32_t x,
                             int32_t y) = 0;

    virtual std::string itemEffectText(int32_t index) = 0;

    virtual std::string npcName(int32_t npc) = 0;

    virtual int32_t runMonsters(GameCanvas &canvas, int64_t nowMs) = 0;

    virtual int8_t campStateFor(GameCanvas &canvas, int8_t state) {
        (void)canvas;
        return state;
    }

    virtual void onCampAmbush(GameCanvas &canvas) = 0;

    virtual void onRespawn(GameCanvas &canvas) { (void)canvas; }

    virtual void beforeWorldTick(GameCanvas &canvas) { (void)canvas; }
    virtual void afterTick(GameCanvas &canvas) { (void)canvas; }
    virtual void onNpcAhead(GameCanvas &canvas, bool npcTileAhead) {
        (void)canvas;
        (void)npcTileAhead;
    }

    virtual void onMonsterSlain(GameCanvas &canvas, Monster &monster) = 0;

    virtual void openNpcScreen(GameCanvas &canvas, int32_t npc) = 0;

    virtual bool crossedBoundary(const Player &player) = 0;
    virtual SharedArray<std::string> arrivalMessage(GameCanvas &canvas) = 0;

    virtual bool talkLayoutForNpc(const GameCanvas &canvas) = 0;

    virtual void onSecond(GameCanvas &canvas) { (void)canvas; }

    virtual void drawNpcSlot(GameCanvas &canvas, Graphics *graphics, const std::string &slot,
                             int32_t n) = 0;

    virtual int32_t wallKind(int8_t tile) = 0;
    virtual void beginWalls(GameCanvas &canvas) { (void)canvas; }
    virtual void drawWallSlice(GameCanvas &canvas, Graphics *graphics, int32_t slice,
                               int32_t x, int32_t kind) = 0;

    virtual void drawObject(GameCanvas &canvas, Graphics *graphics,
                            const SharedArray<int8_t> &record, int32_t slot) = 0;

    virtual bool drawSpecialMonster(GameCanvas &canvas, Graphics *graphics, int32_t type) = 0;

    virtual void drawNpcPortrait(GameCanvas &canvas, Graphics *graphics, int32_t npc) = 0;

    virtual void drawIconRow(GameCanvas &canvas, Graphics *graphics, int32_t layout) = 0;

    virtual void drawEffect(GameCanvas &canvas, Graphics *graphics, int32_t effect, int32_t x,
                            int32_t y) = 0;

    virtual void refreshMinimap(GameCanvas &canvas) = 0;
    virtual void invalidateMinimap(GameCanvas &canvas) = 0;
    virtual void drawMinimap(GameCanvas &canvas, Graphics *graphics) = 0;
};

}

#endif
