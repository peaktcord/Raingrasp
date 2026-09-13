#include "src/common/game/spells.hpp"

#include "src/common/game/util.hpp"

int32_t Spell::count_ = 0;
SharedArray<Spell *> Spell::all_;

Spell *Spell::byId(int32_t id) { return all_[indexOf(id)]; }

bool Spell::isValidId(int32_t id) { return id >= 1 && id <= count_; }

bool Spell::targetsMonster(int32_t id) { return byId(id)->target_ == 2; }

void Spell::load(platform::PlatformContext *context) {
    try {
        BinaryReader *in = GameUtil::openDatFile(context, std::string("spellsin.dat"));
        count_ = in->readShort();
        all_ = SharedArray<Spell *>(count_);
        for (int32_t n = 0; n < count_; ++n) all_[n] = new Spell();
        for (int32_t n = 0; n < count_; ++n) all_[n]->name_ = in->readUTF();
        for (int32_t n = 0; n < count_; ++n) all_[n]->skill_ = in->readByte();
        for (int32_t n = 0; n < count_; ++n) all_[n]->magickaCost_ = in->readByte();
        for (int32_t n = 0; n < count_; ++n) all_[n]->effect_ = in->readByte();
        for (int32_t n = 0; n < count_; ++n) all_[n]->target_ = in->readByte();
        for (int32_t n = 0; n < count_; ++n) all_[n]->difficulty_ = in->readByte();
        for (int32_t n = 0; n < count_; ++n) all_[n]->level_ = in->readByte();
        for (int32_t n = 0; n < count_; ++n) all_[n]->description_ = in->readUTF();
        delete in;
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to load the spell table: ") +
                               exception.what());
    }
}
