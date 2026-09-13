#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "src/common/platform/desktop.hpp"
#include "src/common/replay/headless.hpp"
#include "src/common/game/spells.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/vitals.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/stormhold/variant.hpp"
#include "src/common/game/player.hpp"
#include "src/stormhold/extension.hpp"
#include "src/stormhold/profile.hpp"

using namespace stormhold;

namespace {

std::string g_out;

void row(const std::string &line) {
    g_out += line;
    g_out += "\n";
}

std::string i2s(int32_t v) { return std::to_string(v); }

unsigned long digest(const SharedArray<int8_t> &bytes) {
    unsigned long hash = 2166136261u;
    for (int32_t n1 = 0; n1 < bytes.length(); ++n1) {
        hash = (hash ^ (unsigned char)bytes[n1]) * 16777619u;
    }
    return hash & 0xFFFFFFFFul;
}

std::string shorts(const SharedArray<int16_t> &a) {
    std::string s1;
    for (int32_t n1 = 0; n1 < a.length(); ++n1) {
        if (n1) s1 += ",";
        s1 += std::to_string((int32_t)a[n1]);
    }
    return s1;
}

void field(const char *fixture, const char *name, int32_t before, int32_t after) {
    row(std::string("field\t") + fixture + "\t" + name + "\t" + i2s(before) + "\t" +
        i2s(after) + "\t" + (before == after ? "same" : "LOST"));
}

Player *spreadCharacter(Game *game, int32_t cls, int32_t salt) {
    GameRandom rng(7000 + salt);
    game->worldState_.random = &rng;

    Player *p = new Player(game);
    p->initFromClass(cls);
    p->name_ = "Save" + std::to_string(salt);
    for (int32_t n1 = 0; n1 < p->vitals_.length(); ++n1) p->vitals_[n1] = (int16_t)(201 + n1 * 11);
    for (int32_t n2 = 0; n2 < p->attributes_.length(); ++n2) p->attributes_[n2] = (int16_t)(13 + n2 * 7);
    for (int32_t n3 = 0; n3 < p->skills_.length(); ++n3) {
        for (int32_t k = 0; k < p->skills_[n3].length(); ++k) {
            p->skills_[n3][k] = (int16_t)(2 + n3 * 3 + k * 47);
        }
    }
    p->vitalSeeds_[0] = (int16_t)(71 + salt);
    p->vitalSeeds_[1] = (int16_t)(83 + salt);
    p->gold_ = 123456 + salt;
    p->luck_ = (int16_t)(5 + salt);
    p->levelUpMask_ = (int8_t)0x67;
    p->targetUid_ = (int16_t)(301 + salt);
    p->damageBonus_ = (int16_t)(401 + salt);
    p->potionAttack_ = true;
    p->potionDefence_ = false;
    p->potionEscape_ = true;
    p->blessed_ = false;
    for (int32_t n4 = 0; n4 < p->equipped_.length(); ++n4) p->equipped_[n4] = (int8_t)(1 + n4 * 3);
    for (int32_t n5 = 0; n5 < p->inventory_.length(); ++n5) p->inventory_[n5] = (int8_t)(n5 + 1);
    for (int32_t n6 = 0; n6 < p->itemData_.length(); ++n6) p->itemData_[n6] = 1000 + n6;
    for (int32_t n7 = 0; n7 < p->spellTimers_.length(); ++n7) p->spellTimers_[n7] = (int8_t)(n7 * 2 + 1);

    p->itemCount_ = 19;
    p->knownSpells_ = 0x02468ACE + salt;
    p->readiedSpell_ = 11;
    p->giftPoints_ = (int16_t)(501 + salt);
    p->rumorsHeard_ = (int16_t)(601 + salt);
    p->ailments_ = (int8_t)0x2D;
    p->ailmentTimer4_ = (int16_t)(701 + salt);
    p->ailmentTimer5_ = (int16_t)(801 + salt);
    p->ailmentTimer7_ = (int16_t)(901 + salt);
    p->gridX_ = 29;
    p->gridY_ = 31;
    p->facing_ = 3;
    p->recallFacing_ = 2;
    p->recallDungeon_ = 9;
    p->recallX_ = 37;
    p->recallY_ = 41;
    game->worldState_.random = game->rng_.get();
    return p;
}

void dumpRoundTrip(Game *game, const char *fixture, int32_t cls, int32_t salt, int32_t warden,
                   bool restoreVitals) {
    Player *before = spreadCharacter(game, cls, salt);

    ext(before).wardenStage_ = (int16_t)warden;

    before->dungeonId_ = 1;

    SharedArray<int8_t> bytes = before->toBytes(restoreVitals);
    char hash[32];
    std::snprintf(hash, sizeof(hash), "%08lx", digest(bytes));
    row(std::string("stream\t") + fixture + "\t" + i2s(bytes.length()) + "\t" + hash);

    Player *after = Player::fromBytes(profile(), bytes, restoreVitals);
    if (after == nullptr) {
        row(std::string("load\t") + fixture + "\tNULL");
        return;
    }

    row(std::string("vitals\t") + fixture + "\t" + shorts(before->vitals_) + "\t" +
        shorts(after->vitals_));
    row(std::string("attributes\t") + fixture + "\t" + shorts(before->attributes_) + "\t" +
        shorts(after->attributes_));

    field(fixture, "class", (int32_t)before->classId_, (int32_t)after->classId_);
    field(fixture, "classArg", (int32_t)before->raceId_, (int32_t)after->raceId_);
    field(fixture, "levelUpMask", (int32_t)before->levelUpMask_, (int32_t)after->levelUpMask_);
    field(fixture, "gold", before->gold_, after->gold_);
    field(fixture, "magickaMul", (int32_t)before->luck_, (int32_t)after->luck_);
    field(fixture, "resist0", (int32_t)before->vitalSeeds_[0], (int32_t)after->vitalSeeds_[0]);
    field(fixture, "resist1", (int32_t)before->vitalSeeds_[1], (int32_t)after->vitalSeeds_[1]);
    field(fixture, "itemCount", (int32_t)before->itemCount_, (int32_t)after->itemCount_);
    field(fixture, "knownSpells", before->knownSpells_, after->knownSpells_);
    field(fixture, "readiedSpell", (int32_t)before->readiedSpell_, (int32_t)after->readiedSpell_);
    field(fixture, "giftPoints", (int32_t)before->giftPoints_, (int32_t)after->giftPoints_);
    field(fixture, "rumorsHeard", (int32_t)before->rumorsHeard_, (int32_t)after->rumorsHeard_);
    field(fixture, "ailments", (int32_t)before->ailments_, (int32_t)after->ailments_);
    field(fixture, "ailmentTimer4", (int32_t)before->ailmentTimer4_, (int32_t)after->ailmentTimer4_);
    field(fixture, "ailmentTimer5", (int32_t)before->ailmentTimer5_, (int32_t)after->ailmentTimer5_);
    field(fixture, "ailmentTimer7", (int32_t)before->ailmentTimer7_, (int32_t)after->ailmentTimer7_);
    field(fixture, "blessed", before->blessed_ ? 1 : 0, after->blessed_ ? 1 : 0);
    field(fixture, "dungeonId", (int32_t)before->dungeonId_, (int32_t)after->dungeonId_);
    field(fixture, "gridX", (int32_t)before->gridX_, (int32_t)after->gridX_);
    field(fixture, "gridY", (int32_t)before->gridY_, (int32_t)after->gridY_);
    field(fixture, "facing", (int32_t)before->facing_, (int32_t)after->facing_);
    field(fixture, "recallDungeon", (int32_t)before->recallDungeon_, (int32_t)after->recallDungeon_);
    field(fixture, "recallX", (int32_t)before->recallX_, (int32_t)after->recallX_);
    field(fixture, "recallY", (int32_t)before->recallY_, (int32_t)after->recallY_);
    field(fixture, "recallFacing", (int32_t)before->recallFacing_, (int32_t)after->recallFacing_);
    field(fixture, "z", (int32_t)before->targetUid_, (int32_t)after->targetUid_);
    field(fixture, "v", (int32_t)before->damageBonus_, (int32_t)after->damageBonus_);
    field(fixture, "flagC", before->potionAttack_ ? 1 : 0, after->potionAttack_ ? 1 : 0);
    field(fixture, "flagAc", before->potionDefence_ ? 1 : 0, after->potionDefence_ ? 1 : 0);
    field(fixture, "flagB", before->potionEscape_ ? 1 : 0, after->potionEscape_ ? 1 : 0);

    field(fixture, "wardenStage", (int32_t)ext(before).wardenStage_, (int32_t)ext(after).wardenStage_);

    row(std::string("name\t") + fixture + "\t" + before->name_ + "\t" + after->name_);

    std::string eqBefore, eqAfter;
    for (int32_t n1 = 0; n1 < before->equipped_.length(); ++n1) {
        if (n1) { eqBefore += ","; eqAfter += ","; }
        eqBefore += std::to_string((int32_t)before->equipped_[n1]);
        eqAfter += std::to_string((int32_t)after->equipped_[n1]);
    }
    row(std::string("equip\t") + fixture + "\t" + eqBefore + "\t" + eqAfter);

    std::string invBefore, invAfter;
    for (int32_t n2 = 0; n2 < before->inventory_.length(); ++n2) {
        if (n2) { invBefore += ","; invAfter += ","; }
        invBefore += std::to_string((int32_t)before->inventory_[n2]);
        invAfter += std::to_string((int32_t)after->inventory_[n2]);
    }
    row(std::string("inventory\t") + fixture + "\t" + invBefore);
    row(std::string("inventory_after\t") + fixture + "\t" + invAfter);

    std::string datBefore, datAfter;
    for (int32_t n3 = 0; n3 < before->itemData_.length(); ++n3) {
        if (n3) { datBefore += ","; datAfter += ","; }
        datBefore += std::to_string(before->itemData_[n3]);
        datAfter += std::to_string(after->itemData_[n3]);
    }
    row(std::string("itemdata\t") + fixture + "\t" + datBefore);
    row(std::string("itemdata_after\t") + fixture + "\t" + datAfter);

    std::string tmBefore, tmAfter;
    for (int32_t n4 = 0; n4 < before->spellTimers_.length(); ++n4) {
        if (n4) { tmBefore += ","; tmAfter += ","; }
        tmBefore += std::to_string((int32_t)before->spellTimers_[n4]);
        tmAfter += std::to_string((int32_t)after->spellTimers_[n4]);
    }
    row(std::string("spelltimers\t") + fixture + "\t" + tmBefore);
    row(std::string("spelltimers_after\t") + fixture + "\t" + tmAfter);

    std::string skillsBefore, skillsAfter;
    for (int32_t n5 = 0; n5 < before->skills_.length(); ++n5) {
        for (int32_t k = 0; k < before->skills_[n5].length(); ++k) {
            if (!skillsBefore.empty()) { skillsBefore += ","; skillsAfter += ","; }
            skillsBefore += std::to_string((int32_t)before->skills_[n5][k]);
            skillsAfter += std::to_string((int32_t)after->skills_[n5][k]);
        }
    }
    row(std::string("skills\t") + fixture + "\t" + skillsBefore);
    row(std::string("skills_after\t") + fixture + "\t" + skillsAfter);
}

int emit(const char *mode, const char *path) {
    if (std::string(mode) == "--write") {
        std::FILE *fp = std::fopen(path, "wb");
        if (fp == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", path);
            return 2;
        }
        std::fwrite(g_out.data(), 1, g_out.size(), fp);
        std::fclose(fp);
        std::printf("wrote %s\n", path);
        return 0;
    }
    std::FILE *fp = std::fopen(path, "rb");
    if (fp == nullptr) {
        std::fprintf(stderr, "cannot read baseline %s\n", path);
        return 2;
    }
    std::string expected;
    char buf[4096];
    size_t n1;
    while ((n1 = std::fread(buf, 1, sizeof(buf), fp)) > 0) expected.append(buf, n1);
    std::fclose(fp);

    std::string got = g_out;
    got.erase(std::remove(got.begin(), got.end(), '\r'), got.end());
    expected.erase(std::remove(expected.begin(), expected.end(), '\r'), expected.end());
    if (got == expected) {
        size_t rows = 0;
        for (char c1 : got) {
            if (c1 == '\n') ++rows;
        }
        std::printf("save round-trip matches the baseline (%zu rows)\n", rows);
        return 0;
    }
    std::vector<std::string> a, b;
    for (std::string *src : {&got, &expected}) {
        std::vector<std::string> &out = (src == &got) ? a : b;
        std::string line;
        for (char c2 : *src) {
            if (c2 == '\n') { out.push_back(line); line.clear(); } else { line += c2; }
        }
        if (!line.empty()) out.push_back(line);
    }
    std::printf("FAIL: the save format changed\n");
    std::printf("  NOTE: unlike the Dawnstar baseline, every field here is expected to\n");
    std::printf("        survive -- this format has no lossy pair. A `same` row that\n");
    std::printf("        became LOST is a round-trip regression.\n");
    int shown = 0;
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const std::string &ga = i < a.size() ? a[i] : std::string();
        const std::string &gb = i < b.size() ? b[i] : std::string();
        if (ga != gb) {
            if (shown == 0) std::printf("  first difference at line %zu\n", i + 1);
            if (shown < 5) {
                std::printf("    expected: %s\n", gb.c_str());
                std::printf("    actual:   %s\n", ga.c_str());
            }
            ++shown;
        }
    }
    std::printf("  %d line(s) differ; %zu expected, %zu produced\n", shown, b.size(),
                a.size());
    return 1;
}

}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/rms-savedump-sh");
    stormhold_init_statics(platform::defaultContext());
    Game *game = new Game(profile(), platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();

    row("# save round-trip. Unlike Dawnstar's, this format loses nothing;");
    row("# a LOST row here is a regression. See save_dump.cpp.");

    for (int32_t cls = 0; cls < (int32_t)Player::classCount_; ++cls) {
        std::string a = "class" + i2s(cls);
        dumpRoundTrip(game, a.c_str(), cls, cls, 1000 + cls, true);
    }

    for (int32_t warden = 0; warden < 6; ++warden) {
        std::string fixture = "warden" + i2s(warden);
        dumpRoundTrip(game, fixture.c_str(), 0, 200 + warden, warden, true);
    }

    dumpRoundTrip(game, "warden_wide", 0, 250, 4321, true);
    dumpRoundTrip(game, "warden_wide2", 0, 251, 32767, true);
    dumpRoundTrip(game, "warden_neg", 0, 252, -7, true);

    int status = emit(argv[2], argv[3]);
    std::fflush(stdout);
    return status;
}
