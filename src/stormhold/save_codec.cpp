#include "src/stormhold/save_codec.hpp"
#include "src/common/game/player.hpp"
#include "src/stormhold/extension.hpp"
#include "src/stormhold/profile.hpp"
#include "src/common/game/dungeon_core.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/world.hpp"
#include "src/stormhold/npc_script.hpp"

namespace stormhold {

Player *PlayerSaveCodec::fromBytes(const SharedArray<int8_t> &byArray, bool bl) const {
    int32_t n1;
    Player *j2 = nullptr;
    BinaryReader dataInputStream(byArray);
    j2 = new Player(nullptr, profile());
    Extension &state = ext(j2);
    j2->name_ = dataInputStream.readUTF();
    j2->classId_ = dataInputStream.readShort();
    if (!bl) {
        j2->initFromClass((int32_t)j2->classId_);
        j2->resetForNewLife((int32_t)j2->classId_, false);
    }
    j2->raceId_ = dataInputStream.readShort();
    int32_t n2 = 0;
    while (n2 < 10) {
        j2->vitals_[n2] = dataInputStream.readShort();
        ++n2;
    }
    if (bl) {
        j2->levelUpMask_ = dataInputStream.readByte();
    }
    j2->gold_ = dataInputStream.readInt();
    int32_t n3 = 0;
    while (n3 < 16) {
        j2->attributes_[n3] = dataInputStream.readShort();
        ++n3;
    }
    j2->luck_ = dataInputStream.readShort();
    j2->vitalSeeds_[0] = dataInputStream.readShort();
    j2->vitalSeeds_[1] = dataInputStream.readShort();
    int32_t n4 = 0;
    while (n4 < 14) {
        n1 = 0;
        while (n1 < 3) {
            j2->skills_[n4][n1] = dataInputStream.readShort();
            ++n1;
        }
        ++n4;
    }
    if (bl) {
        j2->itemCount_ = dataInputStream.readByte();
        n1 = 0;
        while (n1 < 24) {
            j2->inventory_[n1] = dataInputStream.readByte();
            ++n1;
        }
        int32_t n5 = 0;
        while (n5 < 24) {
            j2->itemData_[n5] = dataInputStream.readInt();
            ++n5;
        }
        int32_t n6 = 0;
        while (n6 < 7) {
            j2->equipped_[n6] = dataInputStream.readByte();
            ++n6;
        }
        j2->knownSpells_ = dataInputStream.readInt();
        j2->readiedSpell_ = dataInputStream.readByte();
    } else {
        j2->knownSpells_ = dataInputStream.readInt();
    }
    if (bl) {
        j2->giftPoints_ = dataInputStream.readShort();
        j2->rumorsHeard_ = dataInputStream.readShort();
        state.wardenStage_ = dataInputStream.readShort();
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
        n1 = 0;
        while (n1 < 25) {
            j2->spellTimers_[n1] = dataInputStream.readByte();
            ++n1;
        }
        j2->targetUid_ = dataInputStream.readShort();
        j2->damageBonus_ = dataInputStream.readShort();
        j2->potionAttack_ = dataInputStream.readBoolean();
        j2->potionDefence_ = dataInputStream.readBoolean();
        j2->potionEscape_ = dataInputStream.readBoolean();
    }
    return j2;
}

SharedArray<int8_t> PlayerSaveCodec::toBytes(Player &player, bool bl) const {
    int32_t n1;
    int32_t n2;
    Extension &state = ext(player);
    int32_t n3 = player.saveSize(bl);
    BinaryWriter dataOutputStream(n3);
    dataOutputStream.writeUTF(player.name_);
    dataOutputStream.writeShort(player.classId_);
    dataOutputStream.writeShort(player.raceId_);
    if (bl) {
        int32_t n4 = 0;
        while (n4 < 10) {
            dataOutputStream.writeShort(player.vitals_[n4]);
            ++n4;
        }
        dataOutputStream.writeByte(player.levelUpMask_);
    } else {
        SharedArray<int16_t> sArray1(10);
        n2 = 0;
        while (n2 < 10) {
            sArray1[n2] = player.vitals_[n2];
            ++n2;
        }
        player.restoreVitals(sArray1);
        n1 = 0;
        while (n1 < 10) {
            dataOutputStream.writeShort(sArray1[n1]);
            ++n1;
        }
    }
    dataOutputStream.writeInt(player.gold_);
    int32_t n5 = 0;
    while (n5 < 16) {
        dataOutputStream.writeShort(player.attributes_[n5]);
        ++n5;
    }
    dataOutputStream.writeShort(player.luck_);
    dataOutputStream.writeShort(player.vitalSeeds_[0]);
    dataOutputStream.writeShort(player.vitalSeeds_[1]);
    n2 = 0;
    while (n2 < 14) {
        n1 = 0;
        while (n1 < 3) {
            dataOutputStream.writeShort(player.skills_[n2][n1]);
            ++n1;
        }
        ++n2;
    }
    if (bl) {
        dataOutputStream.writeByte(player.itemCount_);
        n1 = 0;
        while (n1 < 24) {
            dataOutputStream.writeByte(player.inventory_[n1]);
            ++n1;
        }
        int32_t n6 = 0;
        while (n6 < 24) {
            dataOutputStream.writeInt(player.itemData_[n6]);
            ++n6;
        }
        int32_t n7 = 0;
        while (n7 < 7) {
            dataOutputStream.writeByte(player.equipped_[n7]);
            ++n7;
        }
        dataOutputStream.writeInt(player.knownSpells_);
        dataOutputStream.writeByte(player.readiedSpell_);
    } else {
        n1 = player.startingSpellMask();
        dataOutputStream.writeInt(n1);
    }
    if (bl) {
        dataOutputStream.writeShort(player.giftPoints_);
        dataOutputStream.writeShort(player.rumorsHeard_);
        dataOutputStream.writeShort(state.wardenStage_);
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
        n1 = 0;
        while (n1 < 25) {
            dataOutputStream.writeByte(player.spellTimers_[n1]);
            ++n1;
        }
        dataOutputStream.writeShort(player.targetUid_);
        dataOutputStream.writeShort(player.damageBonus_);
        dataOutputStream.writeBoolean(player.potionAttack_);
        dataOutputStream.writeBoolean(player.potionDefence_);
        dataOutputStream.writeBoolean(player.potionEscape_);
    }
    return dataOutputStream.toByteArray();
}

void PlayerSaveCodec::readOtherState(game::World &world, const SharedArray<int8_t> &byArray) const {
    worldstate::WorldState &worldState_ = world.worldState();
    BinaryReader dataInputStream(byArray);
    Items::nextId_ = dataInputStream.readShort();
    worldState_.monsterUid = dataInputStream.readShort();
    int32_t n1 = 0;
    while (n1 < 7) {
        worldState_.npcs.npcPresent[n1] = dataInputStream.readBoolean();
        ++n1;
    }
    int32_t n2 = 0;
    while (n2 < 7) {
        worldState_.npcs.firstMeeting[n2] = dataInputStream.readBoolean();
        ++n2;
    }
    int32_t n3 = 0;
    while (n3 < 4) {
        worldState_.npcs.interactionCount[n3] = dataInputStream.readShort();
        ++n3;
    }
    int32_t n4 = 0;
    while (n4 < 4) {
        worldState_.npcs.aidPoints[n4] = dataInputStream.readShort();
        ++n4;
    }
    int32_t n5 = 0;
    while (n5 < 4) {
        worldState_.npcs.suspicion[n5] = dataInputStream.readShort();
        ++n5;
    }
    int32_t n6 = 0;
    while (n6 < 4) {
        worldState_.npcs.befriendDone[n6] = dataInputStream.readByte();
        ++n6;
    }
    int32_t n7 = 0;
    while (n7 < 4) {
        worldState_.npcs.threatenDone[n7] = dataInputStream.readByte();
        ++n7;
    }
    worldState_.npcs.wardenVisits = dataInputStream.readByte();
    worldState_.npcs.wardenPresent = dataInputStream.readBoolean();
    worldState_.npcs.scrapCount = dataInputStream.readShort();
    worldState_.npcs.gemCount = dataInputStream.readShort();
    worldState_.npcs.wardenPending = dataInputStream.readBoolean();
    int32_t n8 = 0;
    while (n8 < 7) {
        if (!worldState_.npcs.npcPresent[n8]) {
            DungeonCore *i2 = world.dungeonAt(1);
            i2->tiles_[NpcSystem::npcGridX_[n8]][NpcSystem::npcGridY_[n8]] = GameUtil::clearFlag((int8_t)32, i2->tiles_[NpcSystem::npcGridX_[n8]][NpcSystem::npcGridY_[n8]]);
        }
        ++n8;
    }
}

SharedArray<int8_t> PlayerSaveCodec::writeOtherState(game::World &world) const {
    worldstate::WorldState &worldState_ = world.worldState();
    BinaryWriter dataOutputStream(60);
    dataOutputStream.writeShort(Items::nextId_);
    dataOutputStream.writeShort(worldState_.monsterUid);
    int32_t n1 = 0;
    while (n1 < 7) {
        dataOutputStream.writeBoolean(worldState_.npcs.npcPresent[n1]);
        ++n1;
    }
    int32_t n2 = 0;
    while (n2 < 7) {
        dataOutputStream.writeBoolean(worldState_.npcs.firstMeeting[n2]);
        ++n2;
    }
    int32_t n3 = 0;
    while (n3 < 4) {
        dataOutputStream.writeShort(worldState_.npcs.interactionCount[n3]);
        ++n3;
    }
    int32_t n4 = 0;
    while (n4 < 4) {
        dataOutputStream.writeShort(worldState_.npcs.aidPoints[n4]);
        ++n4;
    }
    int32_t n5 = 0;
    while (n5 < 4) {
        dataOutputStream.writeShort(worldState_.npcs.suspicion[n5]);
        ++n5;
    }
    int32_t n6 = 0;
    while (n6 < 4) {
        dataOutputStream.writeByte(worldState_.npcs.befriendDone[n6]);
        ++n6;
    }
    int32_t n7 = 0;
    while (n7 < 4) {
        dataOutputStream.writeByte(worldState_.npcs.threatenDone[n7]);
        ++n7;
    }
    dataOutputStream.writeByte(worldState_.npcs.wardenVisits);
    dataOutputStream.writeBoolean(worldState_.npcs.wardenPresent);
    dataOutputStream.writeShort(worldState_.npcs.scrapCount);
    dataOutputStream.writeShort(worldState_.npcs.gemCount);
    dataOutputStream.writeBoolean(worldState_.npcs.wardenPending);
    return dataOutputStream.toByteArray();
}

}
