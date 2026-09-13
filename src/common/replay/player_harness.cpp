#include "src/common/replay/player_harness.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/formulas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/util.hpp"
#include "src/common/runtime.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/save_records.hpp"
#include "src/common/replay/headless.hpp"

namespace player_harness {
namespace {

void row(std::string &out, const std::string &line) {
    out += line;
    out += "\n";
}

std::string i2s(int32_t value) { return std::to_string(value); }

std::string joinShorts(const SharedArray<int16_t> &values) {
    std::string text;
    for (int32_t index = 0; index < values.length(); ++index) {
        if (index) text += ",";
        text += std::to_string((int32_t)values[index]);
    }
    return text;
}

void dumpCharacter(std::string &out, const char *fixture, Player *player) {
    row(out, std::string("vitals\t") + fixture + "\t" +
                 joinShorts(player->vitals_));
    row(out, std::string("attributes\t") + fixture + "\t" +
                 joinShorts(player->attributes_));
    row(out, std::string("resist\t") + fixture + "\t" +
                 joinShorts(player->vitalSeeds_));

    std::string skills;
    for (int32_t index = 0; index < player->skills_.length(); ++index) {
        for (int32_t column = 0; column < player->skills_[index].length(); ++column) {
            if (!skills.empty()) skills += ",";
            skills += std::to_string((int32_t)player->skills_[index][column]);
        }
    }
    row(out, std::string("skills\t") + fixture + "\t" + skills);

    row(out, std::string("scalars\t") + fixture + "\t" +
                 i2s((int32_t)player->classId_) + "\t" +
                 i2s((int32_t)player->raceId_) + "\t" +
                 i2s((int32_t)player->luck_) + "\t" +
                 i2s((int32_t)player->levelUpMask_) + "\t" +
                 i2s(player->gold_) + "\t" + i2s(player->knownSpells_) + "\t" +
                 i2s((int32_t)player->itemCount_));

    row(out, std::string("derived\t") + fixture + "\t" +
                 i2s(player->startingSpellMask()) + "\t" +
                 i2s(player->defenceAptitude()) + "\t" +
                 i2s(player->bestWeaponSkill()) + "\t" +
                 i2s(player->attackSkillIndex()) + "\t" +
                 i2s(player->attackAptitude()) + "\t" +
                 i2s(player->weaponDamage()) + "\t" +
                 i2s(player->defenceSkillIndex()) + "\t" +
                 i2s(player->armourRating()) + "\t" +
                 i2s(player->ailmentCount()) + "\t" +
                 i2s(player->fatigueMultiplier()) + "\t" +
                 i2s(player->nextSpell()) + "\t" +
                 i2s(player->hasRoom()));
}

void dumpSkillRatings(std::string &out, const char *fixture, Player *player) {
    std::string plain;
    std::string boosted;
    std::string caps;
    for (int32_t index = 0; index < 14; ++index) {
        if (index) {
            plain += ",";
            boosted += ",";
            caps += ",";
        }
        plain += std::to_string(player->skillRank(index, false));
        boosted += std::to_string(player->skillRank(index, true));
        caps += std::to_string(player->skillAptitude(index));
    }
    row(out, std::string("skill_plain\t") + fixture + "\t" + plain);
    row(out, std::string("skill_boosted\t") + fixture + "\t" + boosted);
    row(out, std::string("skill_cap\t") + fixture + "\t" + caps);
}

void dumpLevelling(std::string &out, const game::Profile &profile, const char *fixture,
                   Player *player) {
    for (int32_t step = 0; step < 6; ++step) {
        for (int32_t skill = 0; skill < 14; ++skill) {
            int32_t awards = (skill + step * 3) % 17;
            for (int32_t award = 0; award < awards; ++award) {
                player->awardSkillXp(skill, 1);
            }
        }
        bool leveled = profile.promoteOnAward ? player->leveledUp_ : player->applyRankUps();
        player->resetVisible();

        std::string ranks;
        for (int32_t skill = 0; skill < 14; ++skill) {
            if (skill != 0) ranks += ",";
            ranks += i2s((int32_t)player->skills_[skill][0]) + "/" +
                     i2s((int32_t)player->skills_[skill][2]);
        }
        row(out, std::string("level\t") + fixture + "\tstep" + i2s(step) + "\t" +
                     i2s((int32_t)player->vitals_[0]) + "\t" +
                     i2s((int32_t)player->vitals_[1]) + "\t" +
                     i2s((int32_t)player->levelUpMask_) + "\t" +
                     (leveled ? "1" : "0") + "\t" + ranks);
    }
}

void dumpEquipment(std::string &out, const char *fixture, Player *player) {
    const int32_t items[] = {1, 5, 9, 14, 20};
    for (int32_t slot = 0; slot < 7; ++slot) {
        for (int32_t item : items) {
            player->equipped_[slot] = (int8_t)item;
            row(out, std::string("equip\t") + fixture + "\tslot" + i2s(slot) +
                         "\titem" + i2s(item) + "\t" +
                         i2s(player->defenceSkill(true)) + "\t" +
                         i2s(player->attackSkill(true)) + "\t" +
                         i2s(player->defenceAptitude()) + "\t" +
                         i2s(player->attackSkillIndex()));
        }
        player->equipped_[slot] = 0;
    }
}

void dumpCombatPure(std::string &out) {
    const int32_t chances[] = {10, 25, 50, 75, 95};
    const int32_t rolls[] = {1, 20, 50, 80, 100};
    for (int32_t attack : chances) {
        for (int32_t defence : chances) {
            for (int32_t attackRoll : rolls) {
                for (int32_t defenceRoll : rolls) {
                    bool defended = false;
                    int32_t result = GameFormulas::resolveCombatRoll(
                        attack, defence, attackRoll, defenceRoll, &defended);
                    row(out, std::string("roll\t") + i2s(attack) + "\t" +
                                 i2s(defence) + "\t" + i2s(attackRoll) + "\t" +
                                 i2s(defenceRoll) + "\t" + i2s(result) + "\t" +
                                 (defended ? "1" : "0"));
                }
            }
        }
    }

    const int32_t raw[] = {0, 5, 10, 25, 60};
    const int32_t armor[] = {0, 3, 10, 30};
    const int32_t maxHp[] = {20, 100, 255};
    for (int32_t rawDamage : raw) {
        for (int32_t armorValue : armor) {
            for (int32_t maximumHp : maxHp) {
                row(out, std::string("damage\t") + i2s(rawDamage) + "\t" +
                             i2s(armorValue) + "\t" + i2s(maximumHp) + "\t" +
                             i2s(GameFormulas::calcDamage(rawDamage, armorValue,
                                                          maximumHp)));
            }
        }
    }

    for (int32_t weight = 0; weight < 8; ++weight) {
        row(out, std::string("swing\t") + i2s(weight) + "\t" +
                     i2s(GameFormulas::calcSwingFatigueCost(weight)));
    }

    const int64_t spans[] = {0, 250, 1000, 2000, 9999};
    for (int64_t milliseconds : spans) {
        for (int32_t speed = 10; speed <= 90; speed += 40) {
            for (int32_t endurance = 10; endurance <= 90; endurance += 40) {
                row(out, std::string("fatigue\t") +
                             std::to_string((long long)milliseconds) + "\t" +
                             i2s(speed) + "\t" + i2s(endurance) + "\t" +
                             i2s(GameFormulas::calcFatigueRegen(
                                 milliseconds, speed, endurance)));
            }
        }
    }
}

void dumpCombatSeeded(std::string &output) {
    const int64_t seeds[] = {1, 42, 8000};
    for (int64_t seed : seeds) {
        GameRandom random(seed);
        std::string rolls;
        for (int32_t index = 0; index < 24; ++index) {
            if (index) rolls += ",";
            rolls += std::to_string(GameFormulas::resolveCombatRoll(&random, 60, 40));
        }
        row(output, std::string("seeded_roll\t") +
                        std::to_string((long long)seed) + "\t" + rolls);

        GameRandom secondRandom(seed);
        std::string checks;
        for (int32_t skill = 10; skill <= 90; skill += 20) {
            for (int16_t difficulty = 5; difficulty <= 45;
                 difficulty = (int16_t)(difficulty + 20)) {
                if (!checks.empty()) checks += ",";
                checks += std::to_string(
                    GameFormulas::calcInteractionCheck(&secondRandom, skill, difficulty, 40));
            }
        }
        row(output, std::string("seeded_check\t") +
                        std::to_string((long long)seed) + "\t" + checks);
    }
}

int emit(const std::string &out, const char *mode, const char *path) {
    if (std::string(mode) == "--write") {
        std::FILE *file = std::fopen(path, "wb");
        if (file == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", path);
            return 2;
        }
        std::fwrite(out.data(), 1, out.size(), file);
        std::fclose(file);
        std::printf("wrote %s\n", path);
        return 0;
    }

    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot read baseline %s\n", path);
        return 2;
    }
    std::string expected;
    char buffer[4096];
    size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        expected.append(buffer, count);
    }
    std::fclose(file);

    std::string got = out;
    got.erase(std::remove(got.begin(), got.end(), '\r'), got.end());
    expected.erase(std::remove(expected.begin(), expected.end(), '\r'), expected.end());
    if (got == expected) {
        size_t rows = 0;
        for (char value : got) {
            if (value == '\n') ++rows;
        }
        std::printf("player and combat match the baseline (%zu rows)\n", rows);
        return 0;
    }

    std::vector<std::string> actualLines;
    std::vector<std::string> expectedLines;
    for (std::string *source : {&got, &expected}) {
        std::vector<std::string> &lines =
            source == &got ? actualLines : expectedLines;
        std::string line;
        for (char value : *source) {
            if (value == '\n') {
                lines.push_back(line);
                line.clear();
            } else {
                line += value;
            }
        }
        if (!line.empty()) lines.push_back(line);
    }

    std::printf("FAIL: player or combat behaviour changed\n");
    int shown = 0;
    for (size_t index = 0;
         index < actualLines.size() || index < expectedLines.size(); ++index) {
        const std::string &actual =
            index < actualLines.size() ? actualLines[index] : std::string();
        const std::string &expectedLine =
            index < expectedLines.size() ? expectedLines[index] : std::string();
        if (actual != expectedLine) {
            if (shown == 0) {
                std::printf("  first difference at line %zu\n", index + 1);
            }
            if (shown < 5) {
                std::printf("    expected: %s\n", expectedLine.c_str());
                std::printf("    actual:   %s\n", actual.c_str());
            }
            ++shown;
        }
    }
    std::printf("  %d line(s) differ; %zu expected, %zu produced\n", shown,
                expectedLines.size(), actualLines.size());
    return 1;
}

void dumpFreshClasses(std::string &out, const game::Profile &profile, Game *game) {
    GameRandom *sessionRandom = game->worldState_.random;

    for (int32_t classId = 0; classId < (int32_t)Player::classCount_; ++classId) {
        GameRandom random(1000 + classId);
        game->worldState_.random = &random;

        Player *player = new Player(game);
        player->initFromClass(classId);
        player->dungeonId_ = 1;
        std::string fixture = "class" + i2s(classId);
        dumpCharacter(out, fixture.c_str(), player);
        dumpSkillRatings(out, fixture.c_str(), player);
        dumpEquipment(out, fixture.c_str(), player);
        dumpLevelling(out, profile, fixture.c_str(), player);
    }
    game->worldState_.random = sessionRandom;
}

void dumpSpreadClasses(std::string &out, Game *game) {
    GameRandom *sessionRandom = game->worldState_.random;

    for (int32_t classId = 0; classId < (int32_t)Player::classCount_; ++classId) {
        GameRandom random(2000 + classId);
        game->worldState_.random = &random;

        Player *player = new Player(game);
        player->initFromClass(classId);
        for (int32_t index = 0; index < player->vitals_.length(); ++index) {
            player->vitals_[index] = (int16_t)(101 + index * 7);
        }
        for (int32_t index = 0; index < player->attributes_.length(); ++index) {
            player->attributes_[index] = (int16_t)(11 + index * 5);
        }
        for (int32_t skill = 0; skill < player->skills_.length(); ++skill) {
            for (int32_t column = 0; column < player->skills_[skill].length(); ++column) {
                player->skills_[skill][column] =
                    (int16_t)(3 + skill * 3 + column * 43);
            }
        }
        for (int32_t index = 0; index < player->vitalSeeds_.length(); ++index) {
            player->vitalSeeds_[index] = (int16_t)(61 + index * 13);
        }

        std::string fixture = "spread" + i2s(classId);
        dumpCharacter(out, fixture.c_str(), player);
        dumpSkillRatings(out, fixture.c_str(), player);
        dumpEquipment(out, fixture.c_str(), player);
    }
    game->worldState_.random = sessionRandom;
}

}

int run(int argc, char **argv, const game::Profile &profile) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "usage: %s <resource-dir> --check|--write <baseline.tsv>\n",
                     argv[0]);
        return 2;
    }

    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot(std::string("saves/rms-playerdump-") + profile.name);
    profile.initStatics(platform::defaultContext());

    Game *game = new Game(profile, platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();
    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::fprintf(stderr, "FAIL: appload did not complete\n");
        return 1;
    }

    std::string out;
    row(out, "# player: stats, levelling, equipment and combat. See player_dump.cpp.");
    row(out, std::string("classes\t") + i2s((int32_t)Player::classCount_));
    dumpFreshClasses(out, profile, game);
    dumpSpreadClasses(out, game);
    dumpCombatPure(out);
    dumpCombatSeeded(out);

    int status = emit(out, argv[2], argv[3]);
    std::fflush(stdout);
    std::_Exit(status);
}

}
