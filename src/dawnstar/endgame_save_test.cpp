// What survives a save/load around the endgame, and what does not.
//
// Three questions, answered against the real codec rather than by reading it:
//   1. Does the traitor identity survive once the player has asked questions?
//      save_codec.cpp packs traitorId_ and traitorQuestionCount_ into one byte.
//      The original wrote it as `ai << 2 + B`, which Java parses as
//      ai << (2 + B), so the count was always lost and the id shifted out of
//      place whenever the count was nonzero. This walks every (id, count) pair
//      the game can produce and fails if any comes back changed -- the guard
//      that keeps the writer packing the byte the way the reader decodes it.
//   2. Do the endgame progress flags survive -- traitorRevealed_, oracleIndex_,
//      tenacity_? They ride in a trailer the original format never had;
//      oracleIndex_ < 0 disables the oracle countdown outright (extension.cpp
//      onSecond), so losing it strands the run before the boss.
//      A pre-trailer save must still load, with the countdown off as before.
//   3. Same questions asked of the player's actual save file, when one is
//      present, so the answer is not purely synthetic.
#include <cstdio>
#include <string>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/save_codec.hpp"
#include "src/common/game/savegame.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/replay/headless.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/profile.hpp"
#include "src/dawnstar/variant.hpp"

using namespace dawnstar;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string &what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

std::string pair2s(int32_t id, int32_t count) {
    return "traitorId=" + std::to_string(id) + " questionCount=" + std::to_string(count);
}

// A player carried far enough to have endgame state worth losing.
Player *revealedPlayer(Game *game, int32_t traitorId, int32_t questionCount) {
    GameRandom rng(4242);
    game->worldState_.random = &rng;
    Player *p = new Player(game);
    p->initFromClass(0);
    p->dungeonId_ = 1;
    Extension &e = ext(p);
    e.traitorId_ = (int8_t)traitorId;
    e.traitorQuestionCount_ = (int8_t)questionCount;
    e.traitorRevealed_ = true;   // the mystery is solved
    e.oracleIndex_ = 12;         // the countdown to the boss is mid-flight
    e.tenacity_ = true;          // granted alongside the Star of Frost
    e.bossKilled_ = false;
    e.roamerActive_ = false;
    return p;
}

}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <resource-dir> [save-dir]\n", argv[0]);
        return 2;
    }
    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/rms-endgamesave");
    dawnstar_init_statics(platform::defaultContext());
    Game *game = new Game(profile(), platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();
    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::fprintf(stderr, "FAIL: appload did not complete\n");
        return 1;
    }

    // ---- 1. the packed traitor byte, over every pair the game can produce ----
    // traitorQuestionCount_ is clamped to 0..3 by npc_script.cpp; traitorId_ is
    // 0..3 from the roll in onInitFromClass.
    std::printf("traitor identity across save/load:\n");
    std::vector<std::string> corrupted;
    for (int32_t id = 0; id < 4; ++id) {
        for (int32_t count = 0; count <= 3; ++count) {
            Player *before = revealedPlayer(game, id, count);
            SharedArray<int8_t> bytes = before->toBytes(true);
            Player *after = Player::fromBytes(profile(), bytes, true);
            if (after == nullptr) {
                std::printf("  %s -> load returned NULL\n", pair2s(id, count).c_str());
                ++g_failures;
                ++g_checks;
                continue;
            }
            const int32_t gotId = ext(after).traitorId_;
            const int32_t gotCount = ext(after).traitorQuestionCount_;
            const bool same = gotId == id && gotCount == count;
            std::printf("  %-38s -> traitorId=%d questionCount=%d  %s\n",
                        pair2s(id, count).c_str(), gotId, gotCount,
                        same ? "preserved" : "CHANGED");
            if (!same) corrupted.push_back(pair2s(id, count));
        }
    }
    std::printf("  %zu of 16 (id,count) pairs change across a save/load\n", corrupted.size());

    // The traitor is who the player accused; if it moves, the mystery's answer moves.
    check(corrupted.empty(),
          "the traitor identity and question count survive a save/load for all 16 pairs");

    // ---- 2. the endgame progress flags ----
    std::printf("endgame progress across save/load:\n");
    {
        Player *before = revealedPlayer(game, 2, 0);
        SharedArray<int8_t> bytes = before->toBytes(true);
        Player *after = Player::fromBytes(profile(), bytes, true);
        check(after != nullptr, "an endgame save reloads at all");
        if (after != nullptr) {
            const Extension &a = ext(after);
            std::printf("  traitorRevealed: saved=true  loaded=%s\n",
                        a.traitorRevealed_ ? "true" : "false");
            std::printf("  oracleIndex:     saved=12    loaded=%d%s\n", a.oracleIndex_,
                        a.oracleIndex_ < 0 ? "  (< 0 disables the countdown)" : "");
            std::printf("  tenacity:        saved=true  loaded=%s\n",
                        a.tenacity_ ? "true" : "false");

            // These live in the endgame trailer save_codec.cpp appends after the
            // quest flags; the original never saved them. oracleIndex_ is the one
            // that hurts -- onSecond() returns immediately while it is < 0, so a
            // save/load during the countdown used to mean the boss never spawns.
            check(a.traitorRevealed_, "traitorRevealed_ survives a save/load");
            check(a.oracleIndex_ == 12,
                  "oracleIndex_ survives a save/load, so the oracle countdown resumes where it was");
            check(a.tenacity_, "tenacity_ survives a save/load");
        }
    }

    // ---- 2b. a save written before the trailer existed still loads ----
    {
        Player *before = revealedPlayer(game, 1, 0);
        SharedArray<int8_t> bytes = before->toBytes(true);
        SharedArray<int8_t> legacy(bytes.length() - 3);  // drop the trailer
        for (int32_t i = 0; i < legacy.length(); ++i) legacy[i] = bytes[i];
        Player *after = Player::fromBytes(profile(), legacy, true);
        check(after != nullptr, "a pre-trailer save still loads");
        if (after != nullptr) {
            check(ext(after).traitorId_ == 1, "a pre-trailer save keeps its traitor");
            check(!ext(after).traitorRevealed_ && ext(after).oracleIndex_ == -1 && !ext(after).tenacity_,
                  "a pre-trailer save gets the pre-trailer defaults");
        }
    }

    // ---- 3. the player's real save file, if there is one ----
    const char *saveDir = argc >= 3 ? argv[2] : nullptr;
    if (saveDir != nullptr) {
        std::printf("the save file in %s:\n", saveDir);
        SaveRecordFiles::setRoot(saveDir);
        SaveRecords *records = SaveRecords::open(platform::defaultContext(), savegame::kSlotName,
                                                 false);
        if (records == nullptr) {
            std::printf("  no save found -- skipping (synthetic checks above still apply)\n");
        } else {
            SharedArray<int8_t> blob = records->get(1);
            std::printf("  record 1 is %d bytes\n", blob.length());
            Player *loaded = Player::fromBytes(profile(), blob, true);
            if (loaded == nullptr) {
                std::printf("  FAIL: the save did not decode\n");
                ++g_failures;
                ++g_checks;
            } else {
                const Extension &e = ext(loaded);
                std::printf("  name=%s dungeon=%d traitorId=%d questionCount=%d\n",
                            loaded->name_.c_str(), (int32_t)loaded->dungeonId_,
                            (int32_t)e.traitorId_, (int32_t)e.traitorQuestionCount_);
                std::printf("  bossKilled=%d roamerActive=%d traitorRevealed=%d oracleIndex=%d\n",
                            e.bossKilled_ ? 1 : 0, e.roamerActive_ ? 1 : 0,
                            e.traitorRevealed_ ? 1 : 0, e.oracleIndex_);

                // The quest flags are persisted and carry their own record of the
                // interrogation: flag n*18 + what*3 + whom means "asked NPC n", and
                // flag 72 + what*3 + whom is set only when the traitor was asked
                // with traitorQuestionCount_ already at 2 (npc_script.cpp). So each
                // false-clue flag names the traitor at the time of asking, without
                // trusting the packed traitor byte at all.
                int32_t askedPerNpc[4] = {0, 0, 0, 0};
                for (int32_t n = 0; n < 4; ++n)
                    for (int32_t k = 0; k < 18; ++k)
                        if (e.questFlags_[n * 18 + k]) ++askedPerNpc[n];
                std::printf("  questions asked per NPC: [%d, %d, %d, %d]  (index = NPC id)\n",
                            askedPerNpc[0], askedPerNpc[1], askedPerNpc[2], askedPerNpc[3]);
                std::printf("  questions asked of the saved traitorId (%d): %d\n",
                            (int32_t)e.traitorId_, askedPerNpc[e.traitorId_ & 3]);
                int32_t falseClues = 0;
                int32_t liarVotes[4] = {0, 0, 0, 0};
                for (int32_t k = 0; k < 18; ++k) {
                    if (!e.questFlags_[72 + k]) continue;
                    ++falseClues;
                    for (int32_t n = 0; n < 4; ++n)
                        if (e.questFlags_[n * 18 + k]) ++liarVotes[n];
                }
                std::printf("  false-clue flags set: %d  (each needs questionCount >= 2 at save time"
                            " of that answer)\n", falseClues);
                if (falseClues > 0) {
                    std::printf("  NPC that gave the false clues: [%d, %d, %d, %d]\n",
                                liarVotes[0], liarVotes[1], liarVotes[2], liarVotes[3]);
                    int32_t liar = 0;
                    for (int32_t n = 1; n < 4; ++n)
                        if (liarVotes[n] > liarVotes[liar]) liar = n;
                    std::printf("  => the traitor when questioned was NPC %d; the save now says %d\n",
                                liar, (int32_t)e.traitorId_);
                    check(liar == e.traitorId_,
                          "the saved traitorId matches the NPC the false clues came from");
                }

                // How far this character had got: player progress from record 1, and
                // the NPC bookkeeping from the last record (writeOtherState), which
                // counts every conversation regardless of whether questions were asked.
                std::printf("  progress: gold=%d items=%d vitals=[%d,%d,%d]\n", loaded->gold_,
                            (int32_t)loaded->itemCount_, (int32_t)loaded->vitals_[0],
                            (int32_t)loaded->vitals_[1], (int32_t)loaded->vitals_[2]);
                // Item 100 is the Star of Frost, granted only by a correct accusation
                // (extension.cpp giveStarFrost), so it proves who traitorId_ was in
                // memory at that moment, whatever the packed byte says now.
                std::string inv;
                bool starFrost = false;
                for (int32_t i = 0; i < loaded->itemCount_; ++i) {
                    int32_t item = loaded->inventory_[i] < 0 ? -loaded->inventory_[i] : loaded->inventory_[i];
                    if (item == 100) starFrost = true;
                    inv += (i ? "," : "") + std::to_string(item);
                }
                std::printf("  inventory: [%s]  star of frost: %s\n", inv.c_str(),
                            starFrost ? "yes" : "no");
                const int32_t last = records->recordCount();
                std::printf("  records in the save: %d\n", last);
                if (last >= 2) {
                    profile().saveCodec->readOtherState(*game, records->get(last));
                    const worldstate::WorldState &w = game->worldState();
                    std::printf("  NPC conversations: [%d, %d, %d, %d]  aid: [%d, %d, %d, %d]\n",
                                (int32_t)w.npcs.interactionCount[0], (int32_t)w.npcs.interactionCount[1],
                                (int32_t)w.npcs.interactionCount[2], (int32_t)w.npcs.interactionCount[3],
                                (int32_t)w.npcs.aidPoints[0], (int32_t)w.npcs.aidPoints[1],
                                (int32_t)w.npcs.aidPoints[2], (int32_t)w.npcs.aidPoints[3]);
                }

                // Re-saving what we just loaded must be a fixed point. If this
                // drifts, every save/load cycle degrades the file further.
                SharedArray<int8_t> again = loaded->toBytes(true);
                Player *twice = Player::fromBytes(profile(), again, true);
                check(twice != nullptr, "the real save reloads after being re-saved");
                if (twice != nullptr) {
                    const Extension &t = ext(twice);
                    std::printf("  after a re-save: traitorId=%d questionCount=%d\n",
                                (int32_t)t.traitorId_, (int32_t)t.traitorQuestionCount_);
                    check(t.traitorId_ == e.traitorId_,
                          "the real save's traitor identity is stable across a re-save");
                    check(t.traitorQuestionCount_ == e.traitorQuestionCount_,
                          "the real save's question count is stable across a re-save");
                }
            }
            records->close();
        }
    }

    std::printf("%d check(s), %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
