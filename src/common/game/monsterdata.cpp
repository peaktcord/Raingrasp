#include "src/common/game/monsterdata.hpp"

namespace monsterdata {

std::string typeName(const TypeTable &table, int32_t type) {
    return table.names[type - 1];
}

int32_t typeStat(const TypeTable &table, int32_t type, int32_t column) {
    return table.stats[type - 1][column] & 0xFF;
}

bool isUndeadType(int32_t type) {
    return type >= 6 && type <= 8;
}

int32_t applyDamage(int32_t hp, int32_t damage) {
    if (damage > hp) {
        damage = hp;
    }
    return hp - damage;
}

void readFields(BinaryReader *in, Fields *out) {
    out->uid = in->readShort();
    out->type = in->readByte();
    out->hp = in->readByte();
    out->gridX = in->readByte();
    out->gridY = in->readByte();
    out->seen = in->readBoolean();
    out->dungeonId = in->readByte();
    out->moveCounter = in->readByte();
    out->attackPhase = in->readByte();
    out->lastActionMs = in->readLong();
    for (int32_t i = 0; i < kEffectCount; ++i) {
        out->effects[i] = in->readByte();
    }
}

void writeFields(BinaryWriter *out, const Fields &fields) {
    out->writeShort(fields.uid);
    out->writeByte(fields.type);
    out->writeByte(fields.hp);
    out->writeByte(fields.gridX);
    out->writeByte(fields.gridY);
    out->writeBoolean(fields.seen);
    out->writeByte(fields.dungeonId);
    out->writeByte(fields.moveCounter);
    out->writeByte(fields.attackPhase);
    out->writeLong(fields.lastActionMs);
    for (int32_t i = 0; i < kEffectCount; ++i) {
        out->writeByte(fields.effects[i]);
    }
}

}
