#ifndef COMMON_GAME_SPELLS_HPP
#define COMMON_GAME_SPELLS_HPP

#include "src/common/runtime.hpp"
#include "src/common/game/binary_io.hpp"

namespace spellid {

enum SpellId {
    FRENZY = 1,
    SHIELD = 2,
    DEFT_SECURITY = 3,
    WEAKNESS = 4,
    DRAGON_COMBAT = 5,

    DAEDRIC_WEAPON = 6,
    BLOOD_SPIRIT = 7,
    ABSORB = 8,
    DEAD_TO_DUST = 9,
    RIGHTEOUSNESS = 10,

    DAMAGE = 11,
    FEEBLE_BLADE = 12,
    HARM_ARMOR = 13,
    DOOM_HAMMER = 14,
    DRAIN = 15,

    PARALYZE = 16,
    SANCTUARY = 17,
    BLIND = 18,
    FEAR = 19,
    DEATH_HOWL = 20,

    HEAL_WOUND = 21,
    DISARM_TRAP = 22,
    RAISE_ATTRIBUTE = 23,
    CAMP_MAGICKA = 24,
    REMOVE_AILMENT = 25,
};

const int32_t kSpellCount = 25;

}

class Spell {
public:
    std::string name_;
    int8_t skill_ = 0;
    int8_t magickaCost_ = 0;
    int8_t effect_ = 0;
    int8_t target_ = 0;
    int8_t difficulty_ = 0;
    int8_t level_ = 0;
    std::string description_;

    static int32_t count_;
    static SharedArray<Spell *> all_;

    Spell() {}

    static int32_t indexOf(int32_t id) { return id - 1; }
    static Spell *byId(int32_t id);
    static bool isValidId(int32_t id);
    static bool targetsMonster(int32_t id);

    static void load(platform::PlatformContext *context);
};

#endif
