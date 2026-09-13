#ifndef COMMON_GAME_COMBATANT_HPP
#define COMMON_GAME_COMBATANT_HPP

#include "src/common/runtime.hpp"

class Combatant {
public:
    virtual ~Combatant() = default;

    virtual int32_t gridX() const = 0;
    virtual int32_t gridY() const = 0;

    virtual int32_t defenceSkill(bool withAttribute) = 0;
    virtual int32_t defenceAptitude() = 0;
    virtual int32_t defenceSkillIndex() = 0;
    virtual int32_t armourRating() = 0;
    virtual void awardSkillXp(int32_t skill, int32_t points) = 0;

    virtual SharedArray<int16_t> vitals() = 0;
    virtual int8_t ailments() const = 0;
    virtual void setAilments(int8_t bits) = 0;
    virtual void setAilmentTimer(int32_t ailment, int16_t ms) = 0;
};

#endif
