#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "src/common/platform/desktop.hpp"
#include "src/common/replay/headless.hpp"
#include "src/common/game/util.hpp"
#include "src/common/game/vitals.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/common/game/game.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/dawnstar/variant.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/player.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/profile.hpp"

using namespace dawnstar;

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
    p->levelUpMask_ = (int8_t)(salt & 0x7F);
    p->targetUid_ = (int16_t)(301 + salt);
    p->damageBonus_ = (int16_t)(401 + salt);
    p->potionAttack_ = (salt & 1) != 0;
    p->potionDefence_ = (salt & 2) != 0;
    p->potionEscape_ = (salt & 4) != 0;
    ext(p).bossKilled_ = (salt & 8) != 0;
    ext(p).roamerActive_ = (salt & 16) != 0;
    ext(p).traitorRevealed_ = (salt & 32) != 0;
    ext(p).tenacity_ = (salt & 64) != 0;
    ext(p).oracleIndex_ = (salt & 32) != 0 ? 7 + salt : -1;
    for (int32_t n4 = 0; n4 < p->equipped_.length(); ++n4) p->equipped_[n4] = (int8_t)(1 + n4 * 3);
    for (int32_t n5 = 0; n5 < p->inventory_.length(); ++n5) p->inventory_[n5] = (int8_t)(n5 + 1);
    for (int32_t n6 = 0; n6 < p->itemData_.length(); ++n6) p->itemData_[n6] = 1000 + n6;
    for (int32_t n7 = 0; n7 < p->spellTimers_.length(); ++n7) p->spellTimers_[n7] = (int8_t)(n7 * 2 + 1);
    for (int32_t n8 = 0; n8 < ext(p).questFlags_.length(); ++n8) ext(p).questFlags_[n8] = ((n8 * 7) % 3) == 0;
    game->worldState_.random = game->rng_.get();
    return p;
}

void dumpRoundTrip(Game *game, const char *fixture, int32_t cls, int32_t salt, int32_t traitor,
                   int32_t bField, bool restoreVitals) {
    Player *before = spreadCharacter(game, cls, salt);

    ext(before).traitorId_ = (int8_t)traitor;
    ext(before).traitorQuestionCount_ = (int8_t)bField;

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
    field(fixture, "gold", before->gold_, after->gold_);
    field(fixture, "magickaMul", (int32_t)before->luck_, (int32_t)after->luck_);
    field(fixture, "resist0", (int32_t)before->vitalSeeds_[0], (int32_t)after->vitalSeeds_[0]);
    field(fixture, "resist1", (int32_t)before->vitalSeeds_[1], (int32_t)after->vitalSeeds_[1]);
    field(fixture, "z", (int32_t)before->targetUid_, (int32_t)after->targetUid_);
    field(fixture, "v", (int32_t)before->damageBonus_, (int32_t)after->damageBonus_);
    field(fixture, "flagC", before->potionAttack_ ? 1 : 0, after->potionAttack_ ? 1 : 0);
    field(fixture, "flagAc", before->potionDefence_ ? 1 : 0, after->potionDefence_ ? 1 : 0);
    field(fixture, "flagB", before->potionEscape_ ? 1 : 0, after->potionEscape_ ? 1 : 0);
    field(fixture, "flagAj", ext(before).bossKilled_ ? 1 : 0, ext(after).bossKilled_ ? 1 : 0);
    field(fixture, "flagM", ext(before).roamerActive_ ? 1 : 0, ext(after).roamerActive_ ? 1 : 0);
    field(fixture, "traitorRevealed", ext(before).traitorRevealed_ ? 1 : 0, ext(after).traitorRevealed_ ? 1 : 0);
    field(fixture, "tenacity", ext(before).tenacity_ ? 1 : 0, ext(after).tenacity_ ? 1 : 0);
    field(fixture, "oracleIndex", ext(before).oracleIndex_, ext(after).oracleIndex_);

    field(fixture, "traitor_ai", (int32_t)ext(before).traitorId_, (int32_t)ext(after).traitorId_);
    field(fixture, "B", (int32_t)ext(before).traitorQuestionCount_,
          (int32_t)ext(after).traitorQuestionCount_);

    row(std::string("name\t") + fixture + "\t" + before->name_ + "\t" + after->name_);

    std::string eqBefore, eqAfter;
    for (int32_t n1 = 0; n1 < before->equipped_.length(); ++n1) {
        if (n1) { eqBefore += ","; eqAfter += ","; }
        eqBefore += std::to_string((int32_t)before->equipped_[n1]);
        eqAfter += std::to_string((int32_t)after->equipped_[n1]);
    }
    row(std::string("equip\t") + fixture + "\t" + eqBefore + "\t" + eqAfter);

    int32_t bits = 0;
    for (int32_t n2 = 0; n2 < ext(before).questFlags_.length() && n2 < ext(after).questFlags_.length(); ++n2) {
        if (ext(before).questFlags_[n2] != ext(after).questFlags_[n2]) ++bits;
    }
    row(std::string("flagbits\t") + fixture + "\t" + i2s(ext(before).questFlags_.length()) + "\t" +
        i2s(bits) + " differing");

    std::string skillsBefore, skillsAfter;
    for (int32_t n3 = 0; n3 < before->skills_.length(); ++n3) {
        for (int32_t k = 0; k < before->skills_[n3].length(); ++k) {
            if (!skillsBefore.empty()) { skillsBefore += ","; skillsAfter += ","; }
            skillsBefore += std::to_string((int32_t)before->skills_[n3][k]);
            skillsAfter += std::to_string((int32_t)after->skills_[n3][k]);
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
    SaveRecordFiles::setRoot("saves/rms-savedump");
    dawnstar_init_statics(platform::defaultContext());
    Game *game = new Game(profile(), platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();
    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::fprintf(stderr, "FAIL: appload did not complete\n");
        return 1;
    }

    row("# save round-trip. Fields marked LOST do not survive; see save_dump.cpp.");

    for (int32_t cls = 0; cls < (int32_t)Player::classCount_; ++cls) {
        std::string a = "class" + i2s(cls);
        dumpRoundTrip(game, a.c_str(), cls, cls, 1, 0, true);
    }

    for (int32_t ai = 0; ai < 4; ++ai) {
        for (int32_t b = 0; b < 4; ++b) {
            std::string fixture = "shift_ai" + i2s(ai) + "_b" + i2s(b);
            dumpRoundTrip(game, fixture.c_str(), 0, 100 + ai * 4 + b, ai, b, true);
        }
    }

    int status = emit(argv[2], argv[3]);
    std::fflush(stdout);
    std::_Exit(status);
}
