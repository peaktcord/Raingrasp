#ifndef DAWNSTAR_EXTENSION_HPP
#define DAWNSTAR_EXTENSION_HPP

#include "src/common/game/extension.hpp"
#include "src/common/game/player.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

namespace dawnstar {

class Extension : public game::Extension {
public:
    SharedArray<uint8_t> questFlags_ = SharedArray<uint8_t>(96);
    int8_t traitorQuestionCount_ = 0;
    int8_t traitorId_ = 0;
    bool bossKilled_ = false;
    bool roamerActive_ = false;
    bool traitorRevealed_ = false;
    int32_t oracleIndex_ = -1;
    bool tenacity_ = false;

    int32_t skillRankBonus(const Player &player) override;
    void onInitFromClass(Player &player) override;
    void onPlaceAtStart(Player &player) override;
    void onEdgeCandidate(Player &player) override;
    void onRest(Player &player) override;
    bool takeItemsUnderfoot(Player &player, int8_t tile, bool forward) override;
    void placeVisibleNpcs(Player &player) override;
    bool npcPosition(const Player &player, int32_t kind, const visibility::Slot &object, int8_t &x,
                     int8_t &y) override;
    int32_t npcAhead(const Player &player, int32_t dungeonId, int32_t x, int32_t y) override;
    std::string itemEffectText(int32_t index) override;

    int32_t runMonsters(GameCanvas &canvas, int64_t nowMs) override;
    int8_t campStateFor(GameCanvas &canvas, int8_t state) override;
    void onCampAmbush(GameCanvas &canvas) override;
    void onRespawn(GameCanvas &canvas) override;
    void onMonsterSlain(GameCanvas &canvas, Monster &monster) override;
    void openNpcScreen(GameCanvas &canvas, int32_t npc) override;
    bool crossedBoundary(const Player &player) override;
    SharedArray<std::string> arrivalMessage(GameCanvas &canvas) override;
    bool talkLayoutForNpc(const GameCanvas &canvas) override;
    void onSecond(GameCanvas &canvas) override;
    std::string npcName(int32_t npc) override;
    void drawNpcSlot(GameCanvas &canvas, Graphics *graphics, const std::string &slot,
                     int32_t n) override;
    int32_t wallKind(int8_t tile) override;
    void beginWalls(GameCanvas &canvas) override;
    void drawWallSlice(GameCanvas &canvas, Graphics *graphics, int32_t slice, int32_t x,
                       int32_t kind) override;
    void drawObject(GameCanvas &canvas, Graphics *graphics, const SharedArray<int8_t> &record,
                    int32_t slot) override;
    bool drawSpecialMonster(GameCanvas &canvas, Graphics *graphics, int32_t type) override;
    void drawNpcPortrait(GameCanvas &canvas, Graphics *graphics, int32_t npc) override;
    void drawIconRow(GameCanvas &canvas, Graphics *graphics, int32_t layout) override;
    void drawEffect(GameCanvas &canvas, Graphics *graphics, int32_t effect, int32_t x,
                    int32_t y) override;
    void refreshMinimap(GameCanvas &canvas) override;
    void invalidateMinimap(GameCanvas &canvas) override;
    void drawMinimap(GameCanvas &canvas, Graphics *graphics) override;

    static const int32_t kOracleOffsets[12];
    bool sideWallPending_ = false;
    bool frontWallPending_ = false;
    Image *mapImage_ = nullptr;

    void removeRoamer(Player &player);
    void giveStarFrost(Player &player);

private:
    void drawIcon(Graphics *graphics, int32_t n, int32_t x, int32_t y);
    static void drawWallImage(Graphics *graphics, int32_t n3, int32_t x);
    void drawMapFrame(GameCanvas &canvas, Graphics *graphics, int32_t n, int32_t n2, int32_t n3,
                      int32_t n4);
    Image *mapImage(Player &player);
};

inline Extension &ext(Player &player) { return static_cast<Extension &>(player.extension()); }
inline Extension &ext(Player *player) { return ext(*player); }

}
#endif
