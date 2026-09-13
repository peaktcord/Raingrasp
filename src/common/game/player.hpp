#ifndef COMMON_GAME_PLAYER_HPP
#define COMMON_GAME_PLAYER_HPP

#include <memory>
#include <vector>

#include "src/common/game/combatant.hpp"
#include "src/common/game/combatstats.hpp"
#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/extension.hpp"
#include "src/common/game/inventory.hpp"
#include "src/common/game/melee.hpp"
#include "src/common/game/profile.hpp"
#include "src/common/game/progression.hpp"
#include "src/common/game/spellcast.hpp"
#include "src/common/game/visibility.hpp"
#include "src/common/game/world.hpp"
#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

class Monster;

class Player : public Combatant {
public:

    int32_t gridX() const override { return gridX_; }
    int32_t gridY() const override { return gridY_; }
    SharedArray<int16_t> vitals() override { return vitals_; }
    int8_t ailments() const override { return ailments_; }
    void setAilments(int8_t bits) override { ailments_ = bits; }
    void setAilmentTimer(int32_t ailment, int16_t ms) override {
        if (ailment == 4) ailmentTimer4_ = ms;
        if (ailment == 5) ailmentTimer5_ = ms;
    }
    bool gameWon_ = false;
    visibility::Slots visibleSlots_;
    static bool dataLoaded_;
    static int16_t classCount_;
    static SharedArray<std::string> classNames_;
    static SharedArray<std::string> raceNames_;
    static SharedArray<std::string> skillNames_;
    static SharedArray<SharedArray<int16_t>> classTable_;
    static SharedArray<std::string> vitalNames_;
    static SharedArray<std::string> attributeNames_;
    static SharedArray<int16_t> skillAttribute_;
    static const int32_t startingKit_[7][2];

    game::World *world_ = nullptr;
    const game::Profile *profile_ = nullptr;
    std::unique_ptr<game::Extension> extension_;

    std::string name_;
    int16_t classId_ = 0;
    int16_t raceId_ = 0;
    SharedArray<int16_t> vitals_;
    int8_t levelUpMask_ = 0;
    int32_t gold_ = 0;
    SharedArray<int16_t> attributes_;
    int16_t luck_ = 0;
    SharedArray<int16_t> vitalSeeds_;
    SharedArray<SharedArray<int16_t>> skills_;
    int8_t itemCount_ = 0;
    SharedArray<int8_t> inventory_;
    SharedArray<int32_t> itemData_;
    SharedArray<int8_t> equipped_;
    int32_t knownSpells_ = 0;
    int8_t readiedSpell_ = 0;
    int16_t giftPoints_ = 0;
    int16_t rumorsHeard_ = 0;
    int8_t ailments_ = 0;
    int16_t ailmentTimer4_ = 0;
    int16_t ailmentTimer5_ = 0;
    int16_t ailmentTimer7_ = 0;
    bool blessed_ = false;
    int8_t dungeonId_ = 0;
    int8_t gridX_ = 0;
    int8_t gridY_ = 0;
    int8_t facing_ = 0;
    int8_t recallDungeon_ = 0;
    int8_t recallX_ = 0;
    int8_t recallY_ = 0;
    int8_t recallFacing_ = 0;
    SharedArray<int8_t> spellTimers_;
    int16_t targetUid_ = 0;
    int16_t damageBonus_ = 0;
    bool potionAttack_ = false;
    bool potionDefence_ = false;
    bool potionEscape_ = false;
    int8_t nextX_ = 0;
    int8_t nextY_ = 0;
    int8_t nextFacing_ = 0;
    int8_t nextDungeon_ = 0;
    bool crossedEdge_ = false;
    bool warped_ = false;
    SharedArray<SharedArray<int8_t>> surroundings_;
    int8_t prevX_ = 0;
    int8_t prevY_ = 0;
    bool leveledUp_ = false;

    explicit Player(game::World *world);
    Player(game::World *world, const game::Profile &profile);
    ~Player() override;

    game::Extension &extension() { return *extension_; }
    const game::Profile &profile() const { return *profile_; }

    int32_t saveSize(bool bl);
    void restoreVitals(SharedArray<int16_t> sArray);
    void initFromClass(int32_t n);
    void recalcMaxVitals();
    int32_t startingSpellMask();
    void resetForNewLife(int32_t n, bool bl);
    void placeAtStart(bool bl);
    std::string classSummary();
    static void ensureDataLoaded(platform::PlatformContext *context);
    static void loadCharData(platform::PlatformContext *context, const std::string &string);
    static SharedArray<std::string> readStringTable(BinaryReader *dataInputStream);
    static Player *fromBytes(const game::Profile &profile, const SharedArray<int8_t> &byArray,
                             bool bl);
    SharedArray<int8_t> toBytes(bool bl);
    void stepCandidate(int32_t n);
    bool move(int32_t n, bool bl);
    bool tryStep(int32_t n);
    bool isWalkable(int8_t by);
    void attackMonster(Monster *d2);
    void markSeen(int32_t n, const visibility::Slot &object);
    void refreshVisible(bool bl);
    void placeVisibleLayer(int32_t kind);
    int8_t peekAhead(int32_t n, int32_t n2);
    void resetVisible();
    bool placeVisible(int32_t n, const visibility::Slot &object);
    static bool blocksView(const visibility::Slot &object);
    int32_t combatRoll(int32_t n, int32_t n2);
    int32_t skillRank(int32_t n, bool bl);
    int32_t skillAptitude(int32_t n);
    combatstats::Combatant combatant() const;
    int32_t defenceSkill(bool bl) override;
    int32_t defenceAptitude() override;
    int32_t bestWeaponSkill();
    int32_t attackSkillIndex();
    int32_t attackSkill(bool bl);
    int32_t attackAptitude();
    int32_t weaponDamage();
    int32_t defenceSkillIndex() override;
    int32_t armourRating() override;
    int32_t takeChest(const SharedArray<int8_t> &byArray);
    bool spellActive(int32_t n);
    void clearSpell(int32_t n);
    Monster *monsterAhead(Monster *target);
    SharedArray<int8_t> chestAhead();
    int32_t npcAhead();
    int32_t spellSchool(int32_t n);
    void castSelf(int32_t n);
    void castAtMonster(int32_t n, Monster *d2);
    void awardSkillXp(int32_t n, int32_t n2) override;
    bool takeDroppedItem(const SharedArray<int8_t> &byArray);
    bool addItem(int32_t n, int32_t n2, int32_t n3);
    bool removeItem(int32_t n);
    void dropItem(int32_t n);
    bool isEquipped(int32_t n);
    bool equip(int32_t n, bool bl);
    void unequipSlot(int32_t n);
    void addGold(int32_t n);
    bool equipLast(bool bl);
    void unequipItem(int32_t n);
    int32_t findEquipped(int32_t n);
    inventory::Bag bag();
    melee::Attacker attacker();
    spellcast::Caster caster();
    progression::Progress progress();
    progression::Rules progressionRules();
    bool hasRoom();
    std::string describeItem(int32_t n);
    bool learnSpell(int32_t n);
    void useItemOnTarget(int32_t n);
    bool isEquippable(int32_t n);
    bool isUsable(int32_t n);
    bool canLearn(int32_t n);
    int32_t effectiveVital(int32_t n);
    bool hasRecallPoint();
    void warpToCamp(bool bl);
    void recall();
    int32_t ailmentCount();
    void cureOneAilment();
    int32_t fatigueMultiplier();
    DungeonCore *dungeon();
    void refreshSurroundings();
    std::string characterSheet();
    std::vector<std::string> skillList();
    int32_t skillAt(int32_t n);
    std::string describeSkill(int32_t n);
    std::vector<std::string> spellList();
    int32_t spellAt(int32_t n);
    int32_t nextSpell();
    std::string describeSpell(int32_t n);
    void rest(bool bl);
    bool hasAilment(int32_t n);
    void useItem(int32_t n, Monster *d2);
    bool applyRankUps();
    void spendLevelUp();
    SharedArray<std::string> levelUpChoices();
    void giveStartingKit();
    void regenFatigue(int64_t l);
    int32_t npcInteractionCheck(int32_t faction, int32_t n2);
    static void initializeStatics();
};

#endif
