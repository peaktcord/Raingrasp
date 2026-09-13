#ifndef DAWNSTAR_BOT_TRAITOR_HPP
#define DAWNSTAR_BOT_TRAITOR_HPP

#include <set>
#include <string>

#include "src/common/bot/bot.hpp"
#include "src/common/game/menuaction.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/npc_script.hpp"
#include "src/dawnstar/variant.hpp"

namespace dawnstar {

using DawnstarBot = ::bot::Bot<Dungeon>;

struct Deduction {
    bool solved = false;
    int32_t accused = -1;
    int32_t questionsAsked = 0;
    int32_t liesHeard = 0;
};

namespace {

const int32_t kFalseClueFirst = 37;

inline bool isLie(int32_t dialogueIndex) { return dialogueIndex >= kFalseClueFirst; }

}

inline Deduction deduceTraitor(DawnstarBot &bot) {
    Deduction out;
    Variant &variant = static_cast<Variant &>(bot.game()->variant());

    std::set<std::string> lies;
    for (int32_t n = 0; n < 24; ++n) {
        lies.insert(NpcSystem::dialogue_[9][5 + NpcSystem::kClueFalse[n]]);
    }

    for (int32_t npc = 5; npc <= 8 && !out.solved; ++npc) {
        for (int32_t what = 0; what < 6 && !out.solved; ++what) {
            for (int32_t whom = 0; whom < 3 && !out.solved; ++whom) {
                variant.NPCQuestionWhatUI_->contextIndex_ = npc;
                variant.NPCQuestionWhatUI_->setSelectedIndex(what);
                bot.game()->setCurrentDisplay(variant.NPCQuestionWhatUI_);
                bot.game()->commandAction(UIWidget::cmdSelect_, nullptr);

                if (variant.NPCQuestionWhomUI_ == nullptr) continue;
                variant.NPCQuestionWhomUI_->setSelectedIndex(whom);
                bot.game()->commandAction(UIWidget::cmdSelect_, nullptr);
                ++out.questionsAsked;

                const std::optional<std::string> &answer = bot.game()->GenericInfoUI_->prompts_[0];
                if (!answer) continue;
                if (lies.count(*answer) != 0) {
                    ++out.liesHeard;
                    out.accused = npc - 5;
                    out.solved = true;
                }
            }
        }
    }
    return out;
}

inline bool accuse(DawnstarBot &bot, int32_t npc) {
    Player *p = bot.player();
    bot.game()->performMenuAction(menuaction::REVEAL_TRAITOR, true);
    bot.game()->commandAction(UIWidget::cmdOk_, nullptr);
    if (bot.game()->currentUI_ == nullptr) return false;
    bot.game()->currentUI_->setSelectedIndex(0);
    bot.game()->commandAction(UIWidget::cmdSelect_, nullptr);
    if (bot.game()->currentUI_ == nullptr) return false;
    bot.game()->currentUI_->setSelectedIndex(npc);
    bot.game()->commandAction(UIWidget::cmdSelect_, nullptr);
    const bool right = ext(p).traitorRevealed_;
    bot.game()->commandAction(UIWidget::cmdOk_, nullptr);
    return right;
}

}

#endif
