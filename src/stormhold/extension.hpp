#ifndef STORMHOLD_EXTENSION_HPP
#define STORMHOLD_EXTENSION_HPP

#include "src/common/game/extension.hpp"
#include "src/common/game/player.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

namespace stormhold {

class Extension : public game::Extension {
public:
    int16_t wardenStage_ = 0;
    bool enteredSafeZone_ = false;
    bool leftSafeZone_ = false;

    void onNewLife(Player &player) override;
    void beforeMove(Player &player) override;
    void beforeStep(Player &player) override;
    bool takeItemsUnderfoot(Player &player, int8_t tile, bool forward) override;
    void onGiftPoints(Player &player) override;
    void placeVisibleNpcs(Player &player) override;
    bool npcPosition(const Player &player, int32_t kind, const visibility::Slot &object, int8_t &x,
                     int8_t &y) override;
    int32_t npcAhead(const Player &player, int32_t dungeonId, int32_t x, int32_t y) override;
    std::string itemEffectText(int32_t index) override;

    int32_t runMonsters(GameCanvas &canvas, int64_t nowMs) override;
    void onCampAmbush(GameCanvas &canvas) override;
    void beforeWorldTick(GameCanvas &canvas) override;
    void afterTick(GameCanvas &canvas) override;
    void onNpcAhead(GameCanvas &canvas, bool npcTileAhead) override;
    void onMonsterSlain(GameCanvas &canvas, Monster &monster) override;
    void openNpcScreen(GameCanvas &canvas, int32_t npc) override;
    bool crossedBoundary(const Player &player) override;
    SharedArray<std::string> arrivalMessage(GameCanvas &canvas) override;
    bool talkLayoutForNpc(const GameCanvas &canvas) override;
    std::string npcName(int32_t npc) override;
    void drawNpcSlot(GameCanvas &canvas, Graphics *graphics, const std::string &slot,
                     int32_t n) override;
    int32_t wallKind(int8_t tile) override;
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

    bool wardenHandoffPending_ = false;

    static bool isSafeZone(int32_t dungeonId, int32_t x, int32_t y);
    static bool enchantItem(Player &player, int32_t n);
    static bool isEnchanted(Player &player, int32_t n);
    static int32_t itemTier(Player &player, int32_t n);
    std::string debugDump(Player &player);
    static std::string describeRecord(const SharedArray<int8_t> &byArray);

private:
    static void turnLeft(Player &player);
    static void turnRight(Player &player);
    bool inWardensCamp(const GameCanvas &canvas) const;
    static bool isCrystal(const SharedArray<int8_t> &record);
    void drawWardenPortrait(GameCanvas &canvas, Graphics *graphics, int32_t n);
};

inline Extension &ext(Player &player) { return static_cast<Extension &>(player.extension()); }
inline Extension &ext(Player *player) { return ext(*player); }

}
#endif
