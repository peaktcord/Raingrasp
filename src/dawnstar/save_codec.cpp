#include "src/dawnstar/save_codec.hpp"
#include "src/common/game/player.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/profile.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/world.hpp"

namespace dawnstar {

Player *PlayerSaveCodec::fromBytes(const SharedArray<int8_t> &byArray, bool bl) const {
    int32_t n1;
    int32_t n2;
    int32_t n3;
    Player *j2 = nullptr;
    BinaryReader dataInputStream(byArray);
    j2 = new Player(nullptr, profile());
    Extension &state = ext(j2);
    j2->name_ = dataInputStream.readUTF();
    j2->classId_ = dataInputStream.readShort();
    if (!bl) {
        j2->initFromClass(j2->classId_);
        j2->resetForNewLife(j2->classId_, false);
    }
    j2->raceId_ = dataInputStream.readShort();
    int32_t n4 = 0;
    while (n4 < 10) {
        j2->vitals_[n4] = dataInputStream.readShort();
        ++n4;
    }
    if (bl) {
        j2->levelUpMask_ = dataInputStream.readByte();
    }
    j2->gold_ = dataInputStream.readInt();
    int32_t n5 = 0;
    while (n5 < 16) {
        j2->attributes_[n5] = dataInputStream.readShort();
        ++n5;
    }
    j2->luck_ = dataInputStream.readShort();
    j2->vitalSeeds_[0] = dataInputStream.readShort();
    j2->vitalSeeds_[1] = dataInputStream.readShort();
    int32_t n6 = 0;
    while (n6 < 14) {
        n3 = 0;
        while (n3 < 3) {
            j2->skills_[n6][n3] = dataInputStream.readShort();
            ++n3;
        }
        ++n6;
    }
    if (bl) {
        j2->itemCount_ = dataInputStream.readByte();
        n3 = 0;
        while (n3 < 24) {
            j2->inventory_[n3] = dataInputStream.readByte();
            ++n3;
        }
        n2 = 0;
        while (n2 < 24) {
            j2->itemData_[n2] = dataInputStream.readInt();
            ++n2;
        }
        n1 = 0;
        while (n1 < 7) {
            j2->equipped_[n1] = dataInputStream.readByte();
            ++n1;
        }
        j2->knownSpells_ = dataInputStream.readInt();
        j2->readiedSpell_ = dataInputStream.readByte();
    } else {
        j2->knownSpells_ = dataInputStream.readInt();
    }
    if (bl) {
        j2->giftPoints_ = dataInputStream.readShort();
        j2->rumorsHeard_ = dataInputStream.readShort();
        j2->ailments_ = dataInputStream.readByte();
        j2->ailmentTimer4_ = dataInputStream.readShort();
        j2->ailmentTimer5_ = dataInputStream.readShort();
        j2->ailmentTimer7_ = dataInputStream.readShort();
        j2->blessed_ = dataInputStream.readBoolean();
        j2->dungeonId_ = dataInputStream.readByte();
        j2->gridX_ = dataInputStream.readByte();
        j2->gridY_ = dataInputStream.readByte();
        j2->facing_ = dataInputStream.readByte();
        j2->recallDungeon_ = dataInputStream.readByte();
        j2->recallX_ = dataInputStream.readByte();
        j2->recallY_ = dataInputStream.readByte();
        j2->recallFacing_ = dataInputStream.readByte();
        n3 = 0;
        while (n3 < 25) {
            j2->spellTimers_[n3] = dataInputStream.readByte();
            ++n3;
        }
        j2->targetUid_ = dataInputStream.readShort();
        j2->damageBonus_ = dataInputStream.readShort();
        j2->potionAttack_ = dataInputStream.readBoolean();
        j2->potionDefence_ = dataInputStream.readBoolean();
        j2->potionEscape_ = dataInputStream.readBoolean();
        n2 = dataInputStream.readByte();
        state.roamerActive_ = (n2 & 0x20) == 32;
        state.bossKilled_ = (n2 & 0x10) == 16;
        state.traitorQuestionCount_ = (int8_t)(n2 % 4);
        state.traitorId_ = (int8_t)((n2 >> 2) % 4);
        platform::writeLogLine("Save: traitor is npc " + std::to_string((int32_t)state.traitorId_));
        n1 = 0;
        while (n1 < 96) {
            n2 = dataInputStream.readByte();
            state.questFlags_[n1++] = (n2 & 0x80) != 0;
            state.questFlags_[n1++] = (n2 & 0x40) != 0;
            state.questFlags_[n1++] = (n2 & 0x20) != 0;
            state.questFlags_[n1++] = (n2 & 0x10) != 0;
            state.questFlags_[n1++] = (n2 & 8) != 0;
            state.questFlags_[n1++] = (n2 & 4) != 0;
            state.questFlags_[n1++] = (n2 & 2) != 0;
            state.questFlags_[n1++] = (n2 & 1) != 0;
        }
        // Endgame trailer. The original never wrote traitorRevealed_, oracleIndex_
        // or tenacity_, so a save/load during the oracle countdown left the boss
        // unable to spawn (onSecond is a no-op while oracleIndex_ < 0). Saves from
        // before the trailer simply end here; read() returns -1 at EOF, so they
        // keep the defaults they always had.
        int32_t trailer = dataInputStream.read();
        if (trailer >= 0) {
            state.traitorRevealed_ = (trailer & 1) != 0;
            state.tenacity_ = (trailer & 2) != 0;
            state.oracleIndex_ = dataInputStream.readShort();
        }
    }
    return j2;
}

SharedArray<int8_t> PlayerSaveCodec::toBytes(Player &player, bool bl) const {
    int32_t n1;
    int32_t n2;
    int32_t n3;
    int32_t n4;
    Extension &state = ext(player);
    int32_t n5 = player.saveSize(bl);
    BinaryWriter dataOutputStream(n5);
    dataOutputStream.writeUTF(player.name_);
    dataOutputStream.writeShort(player.classId_);
    dataOutputStream.writeShort(player.raceId_);
    if (bl) {
        int32_t n6 = 0;
        while (n6 < 10) {
            dataOutputStream.writeShort(player.vitals_[n6]);
            ++n6;
        }
        dataOutputStream.writeByte(player.levelUpMask_);
    } else {
        SharedArray<int16_t> sArray1(10);
        n4 = 0;
        while (n4 < 10) {
            sArray1[n4] = player.vitals_[n4];
            ++n4;
        }
        player.restoreVitals(sArray1);
        n3 = 0;
        while (n3 < 10) {
            dataOutputStream.writeShort(sArray1[n3]);
            ++n3;
        }
    }
    dataOutputStream.writeInt(player.gold_);
    int32_t n7 = 0;
    while (n7 < 16) {
        dataOutputStream.writeShort(player.attributes_[n7]);
        ++n7;
    }
    dataOutputStream.writeShort(player.luck_);
    dataOutputStream.writeShort(player.vitalSeeds_[0]);
    dataOutputStream.writeShort(player.vitalSeeds_[1]);
    n4 = 0;
    while (n4 < 14) {
        n3 = 0;
        while (n3 < 3) {
            dataOutputStream.writeShort(player.skills_[n4][n3]);
            ++n3;
        }
        ++n4;
    }
    if (bl) {
        dataOutputStream.writeByte(player.itemCount_);
        n3 = 0;
        while (n3 < 24) {
            dataOutputStream.writeByte(player.inventory_[n3]);
            ++n3;
        }
        n2 = 0;
        while (n2 < 24) {
            dataOutputStream.writeInt(player.itemData_[n2]);
            ++n2;
        }
        n1 = 0;
        while (n1 < 7) {
            dataOutputStream.writeByte(player.equipped_[n1]);
            ++n1;
        }
        dataOutputStream.writeInt(player.knownSpells_);
        dataOutputStream.writeByte(player.readiedSpell_);
    } else {
        n3 = player.startingSpellMask();
        dataOutputStream.writeInt(n3);
    }
    if (bl) {
        dataOutputStream.writeShort(player.giftPoints_);
        dataOutputStream.writeShort(player.rumorsHeard_);
        dataOutputStream.writeByte(player.ailments_);
        dataOutputStream.writeShort(player.ailmentTimer4_);
        dataOutputStream.writeShort(player.ailmentTimer5_);
        dataOutputStream.writeShort(player.ailmentTimer7_);
        dataOutputStream.writeBoolean(player.blessed_);
        dataOutputStream.writeByte(player.dungeonId_);
        dataOutputStream.writeByte(player.gridX_);
        dataOutputStream.writeByte(player.gridY_);
        dataOutputStream.writeByte(player.facing_);
        dataOutputStream.writeByte(player.recallDungeon_);
        dataOutputStream.writeByte(player.recallX_);
        dataOutputStream.writeByte(player.recallY_);
        dataOutputStream.writeByte(player.recallFacing_);
        n3 = 0;
        while (n3 < 25) {
            dataOutputStream.writeByte(player.spellTimers_[n3]);
            ++n3;
        }
        dataOutputStream.writeShort(player.targetUid_);
        dataOutputStream.writeShort(player.damageBonus_);
        dataOutputStream.writeBoolean(player.potionAttack_);
        dataOutputStream.writeBoolean(player.potionDefence_);
        dataOutputStream.writeBoolean(player.potionEscape_);
        n2 = (int8_t)(shiftLeft32(state.traitorId_, 2) + state.traitorQuestionCount_);
        if (state.bossKilled_) {
            n2 = (int8_t)(n2 + 16);
        }
        if (state.roamerActive_) {
            n2 = (int8_t)(n2 + 32);
        }
        dataOutputStream.writeByte(n2);
        n1 = 0;
        while (n1 < 96) {
            n2 = state.questFlags_[n1++] ? -128 : 0;
            n2 = (int8_t)(n2 + (state.questFlags_[n1++] ? 64 : 0));
            n2 = (int8_t)(n2 + (state.questFlags_[n1++] ? 32 : 0));
            n2 = (int8_t)(n2 + (state.questFlags_[n1++] ? 16 : 0));
            n2 = (int8_t)(n2 + (state.questFlags_[n1++] ? 8 : 0));
            n2 = (int8_t)(n2 + (state.questFlags_[n1++] ? 4 : 0));
            n2 = (int8_t)(n2 + (state.questFlags_[n1++] ? 2 : 0));
            n2 = (int8_t)(n2 + (state.questFlags_[n1++] ? 1 : 0));
            dataOutputStream.writeByte(n2);
        }
        // Endgame trailer; see fromBytes.
        int32_t trailer = (state.traitorRevealed_ ? 1 : 0) | (state.tenacity_ ? 2 : 0);
        dataOutputStream.writeByte((int8_t)trailer);
        dataOutputStream.writeShort((int16_t)state.oracleIndex_);
    }
    return dataOutputStream.toByteArray();
}

void PlayerSaveCodec::readOtherState(game::World &world, const SharedArray<int8_t> &byArray) const {
    worldstate::WorldState &worldState_ = world.worldState();
    BinaryReader dataInputStream(byArray);
    Items::nextId_ = dataInputStream.readShort();
    worldState_.monsterUid = dataInputStream.readShort();
    int32_t n1 = 0;
    while (n1 < 9) {
        worldState_.npcs.firstMeeting[n1] = dataInputStream.readBoolean();
        ++n1;
    }
    int32_t n2 = 0;
    while (n2 < 4) {
        worldState_.npcs.interactionCount[n2] = dataInputStream.readShort();
        ++n2;
    }
    int32_t n3 = 0;
    while (n3 < 4) {
        worldState_.npcs.aidPoints[n3] = dataInputStream.readShort();
        ++n3;
    }
    int32_t n4 = 0;
    while (n4 < 4) {
        worldState_.npcs.befriendDone[n4] = dataInputStream.readByte();
        ++n4;
    }
    int32_t n5 = 0;
    while (n5 < 4) {
        worldState_.npcs.threatenDone[n5] = dataInputStream.readByte();
        ++n5;
    }
    worldState_.npcs.wardenPending = dataInputStream.readBoolean();
}

SharedArray<int8_t> PlayerSaveCodec::writeOtherState(game::World &world) const {
    worldstate::WorldState &worldState_ = world.worldState();
    BinaryWriter var1(60);
    var1.writeShort(Items::nextId_);
    var1.writeShort(worldState_.monsterUid);
    int32_t var3 = 0;
    while (var3 < 9) {
        var1.writeBoolean(worldState_.npcs.firstMeeting[var3]);
        ++var3;
    }
    int32_t var4 = 0;
    while (var4 < 4) {
        var1.writeShort(worldState_.npcs.interactionCount[var4]);
        ++var4;
    }
    int32_t var5 = 0;
    while (var5 < 4) {
        var1.writeShort(worldState_.npcs.aidPoints[var5]);
        ++var5;
    }
    int32_t var6 = 0;
    while (var6 < 4) {
        var1.writeByte(worldState_.npcs.befriendDone[var6]);
        ++var6;
    }
    int32_t var7 = 0;
    while (var7 < 4) {
        var1.writeByte(worldState_.npcs.threatenDone[var7]);
        ++var7;
    }
    var1.writeBoolean(worldState_.npcs.wardenPending);
    return var1.toByteArray();
}

}
