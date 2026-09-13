#include "src/common/game/game.hpp"
#include "src/stormhold/profile.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/variant.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/ui_widget.hpp"

#include <iostream>

using namespace stormhold;

namespace {

int failures = 0;
int firstHookCalls[3] = {};
int secondHookCalls[3] = {};

void firstHelperHook(Game *, int32_t) { ++firstHookCalls[0]; }
void firstCanvasHook(GameCanvas *) { ++firstHookCalls[1]; }
void firstSplashHook(UIWidget *) { ++firstHookCalls[2]; }
void secondHelperHook(Game *, int32_t) { ++secondHookCalls[0]; }
void secondCanvasHook(GameCanvas *) { ++secondHookCalls[1]; }
void secondSplashHook(UIWidget *) { ++secondHookCalls[2]; }

void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}

int main() {
    platform::PlatformContext firstContext;
    platform::PlatformContext secondContext;
    Game *firstGame = nullptr;
    Game *secondGame = nullptr;
    Displayable *firstDisplayable = new Form(std::string("First session"));

    SharedArray<int8_t> firstEncodedMonster(28);
    SharedArray<int8_t> secondEncodedMonster(28);
    firstEncodedMonster[1] = 11;
    secondEncodedMonster[1] = 22;
    Monster firstDecodedMonster;
    Monster secondDecodedMonster;
    Monster::fromRecord(&firstDecodedMonster, firstEncodedMonster);
    Monster::fromRecord(&secondDecodedMonster, secondEncodedMonster);
    check(firstDecodedMonster.uid_ == 11 && secondDecodedMonster.uid_ == 22,
          "caller-owned monster decodes do not alias");

    {
        firstGame = new Game(profile(), &firstContext);
        check(firstGame->uiCanvas_ == firstGame->uiCanvasStorage_.get(),
              "game exposes its uniquely owned UI canvas through an observer pointer");
        firstGame->setExecutionHooks(firstHelperHook, firstCanvasHook, firstSplashHook);
        firstGame->createGameCanvas();
        check(firstGame->gameCanvas_ == firstGame->gameCanvasStorage_.get(),
              "game exposes its uniquely owned gameplay canvas");
        check(firstGame->gameCanvas_->combatMonsterStorage_ != nullptr &&
                  firstGame->gameCanvas_->combatMonster_ == nullptr,
              "gameplay canvas owns its combat-record scratch monster, and has no target yet");
        firstGame->startHelperJob(9);
        firstGame->gameCanvas_->startLoop();
        UIWidget firstSplash(firstGame, 2, 1);
        firstSplash.startThread();
        check(firstHookCalls[0] == 1 && firstHookCalls[1] == 1 && firstHookCalls[2] == 1,
              "first session dispatches through its execution hooks");
        firstGame->gameCanvas_->wantAttack_ = true;
        firstGame->gameCanvas_->messageVisible_ = true;
        firstGame->gameCanvas_->mapGrid17_[0][0] = 7;
        firstGame->currentUI_ = firstGame->makeOwnedUIWidget(0, 88);
        check(firstGame->ownedUiWidgets_.back().get() == firstGame->currentUI_,
              "game retains native ownership of its current UI widget");
        firstGame->reloadGame_ = true;
        firstGame->helperThreadState_ = 4;
        firstGame->displayStorage_.setCurrent(firstDisplayable);
        check(variantOf(*firstGame).dungeonGen_.campWidth_ == 19 && variantOf(*firstGame).dungeonGen_.campHeight_ == 19,
              "first session owns a constructed camp grid");
        variantOf(*firstGame).dungeonGen_.campGrid_[0][0] = 7;
        check(firstGame->worldState_.nextMonsterUid() == 1,
              "first session starts its monster UID sequence");
        check(firstGame->worldState_.npcs.firstMeeting[6] &&
                  firstGame->worldState_.npcs.npcPresent[6],
              "Stormhold initializes every NPC as present and unmet");
        Dungeon *firstDungeon = firstGame->dungeons_.emplace<Dungeon>(
            0, firstGame->dungeons_, firstGame->worldState_);
        check(firstGame->dungeons_[0] == firstDungeon,
              "game owns its native dungeon registry entry");
        SharedArray<int8_t> firstRecord(7);
        firstRecord[2] = 1;
        firstGame->worldState_.droppedItems.add(0, firstRecord);
        SharedArray<int8_t> equalValue(7);
        equalValue[2] = 1;
        check(!firstGame->worldState_.droppedItems.removeSame(0, equalValue),
              "removal retains translated array-identity semantics");
        check(!firstGame->worldState_.chests.hasTable(0),
              "Stormhold preserves the absent camp chest table");
        SharedArray<int8_t> firstChest(8);
        firstChest[0] = 3;
        firstChest[1] = 4;
        firstChest[7] = 1;
        firstGame->worldState_.chests.put(1, firstChest);
        SharedArray<int8_t> replacementChest(8);
        replacementChest[0] = 3;
        replacementChest[1] = 4;
        replacementChest[7] = 2;
        firstGame->worldState_.chests.put(1, replacementChest);
        check(firstGame->worldState_.chests.at(1).size() == 1,
              "coordinate replacement does not append a second chest");
        check(firstGame->worldState_.chests.findAt(1, 3, 4).sameRef(replacementChest),
              "coordinate lookup returns the replacement record");
        check(!firstGame->worldState_.monsters.hasTable(0),
              "Stormhold preserves the absent camp monster table");
        SharedArray<int8_t> firstMonster(28);
        firstMonster[1] = 1;
        firstMonster[4] = 6;
        firstMonster[5] = 7;
        firstGame->worldState_.monsters.put(1, firstMonster);
        SharedArray<int8_t> replacementMonster(28);
        replacementMonster[1] = 1;
        replacementMonster[4] = 8;
        replacementMonster[5] = 9;
        firstGame->worldState_.monsters.put(1, replacementMonster);
        firstGame->worldState_.npcs.wardenVisits = 2;
        firstGame->worldState_.npcs.wardenPresent = true;
        firstGame->worldState_.npcs.wardenPending = true;
        firstGame->worldState_.npcs.firstMeeting[0] = false;
        firstGame->worldState_.npcs.npcPresent[1] = false;
        firstGame->worldState_.npcs.befriendDone[0] = 1;
        firstGame->worldState_.npcs.threatenDone[1] = 2;
        firstGame->worldState_.npcs.interactionCount[2] = 3;
        firstGame->worldState_.npcs.aidPoints[3] = 4;
        firstGame->worldState_.npcs.suspicion[0] = 5;
        firstGame->worldState_.npcs.scrapCount = 6;
        firstGame->worldState_.npcs.gemCount = 7;
        check(firstGame->worldState_.monsters.at(1).size() == 1,
              "Stormhold replaces monsters by UID");
        check(firstGame->worldState_.monsters.findUid(1, 1).sameRef(replacementMonster),
              "Stormhold UID lookup returns the moved replacement monster");
        check(firstGame->worldState_.monsters.findAt(1, 8, 9).sameRef(replacementMonster),
              "Stormhold coordinate scan finds the moved monster");
        check(firstGame->platformContext_ == &firstContext,
              "first game retains its explicit platform context");
        check(firstGame->worldState_.random == firstGame->rng_.get(),
              "first world state binds its game-owned random generator");
    }
    {
        secondGame = new Game(profile(), &secondContext);
        check(secondGame->uiCanvas_ == secondGame->uiCanvasStorage_.get(),
              "second game exposes its own uniquely owned UI canvas");
        secondGame->setExecutionHooks(secondHelperHook, secondCanvasHook, secondSplashHook);
        secondGame->createGameCanvas();
        check(secondGame->gameCanvas_ == secondGame->gameCanvasStorage_.get(),
              "second game exposes its own uniquely owned gameplay canvas");
        secondGame->startHelperJob(8);
        secondGame->gameCanvas_->startLoop();
        UIWidget secondSplash(secondGame, 2, 1);
        secondSplash.startThread();
        check(secondHookCalls[0] == 1 && secondHookCalls[1] == 1 && secondHookCalls[2] == 1 &&
                  firstHookCalls[0] == 1 && firstHookCalls[1] == 1 && firstHookCalls[2] == 1,
              "second session dispatches without overwriting first-session hooks");
        secondGame->helperThreadState_ = 1;
        check(!secondGame->gameCanvas_->wantAttack_ &&
                  !secondGame->gameCanvas_->messageVisible_ &&
                  secondGame->gameCanvas_->mapGrid17_[0][0] == 0 &&
                  secondGame->gameCanvas_->combatMonsterStorage_.get() !=
                      firstGame->gameCanvas_->combatMonsterStorage_.get(),
              "second session owns independent canvas runtime state");
        check(secondGame->currentUI_ == nullptr && !secondGame->reloadGame_ &&
                  secondGame->helperThreadState_ == 1,
              "second session owns independent UI and lifecycle state");
        check(&secondGame->displayStorage_ != &firstGame->displayStorage_ &&
                  secondGame->displayStorage_.getCurrent() == nullptr,
              "second session owns an independent display");
        check(secondGame->uiCanvas_ != firstGame->uiCanvas_,
              "second session owns an independent UI canvas");
        check(variantOf(*secondGame).dungeonGen_.campGrid_[0][0] == 1,
              "second session owns an independent camp grid");
        check(secondGame->worldState_.nextMonsterUid() == 1,
              "second session starts an independent monster UID sequence");
        Dungeon *secondDungeon = secondGame->dungeons_.emplace<Dungeon>(
            0, secondGame->dungeons_, secondGame->worldState_);
        check(secondDungeon != firstGame->dungeons_[0],
              "second game owns an independent dungeon instance");
        secondGame->worldState_.droppedItems.add(0, SharedArray<int8_t>(7));
        secondGame->worldState_.droppedItems.add(0, SharedArray<int8_t>(7));
        check(secondGame->platformContext_ == &secondContext,
              "second game retains its explicit platform context");
        check(secondGame->worldState_.random == secondGame->rng_.get(),
              "second world state binds its game-owned random generator");
        check(secondGame->worldState_.random != firstGame->worldState_.random,
              "sessions own independent random generators");
        check(secondGame->worldState_.droppedItems.at(0).size() == 2,
              "second context owns an independent dropped-item list");
        check(secondGame->worldState_.chests.at(1).empty(),
              "second context owns an independent chest table");
        check(secondGame->worldState_.monsters.at(1).empty(),
              "second context owns an independent monster table");
        check(secondGame->worldState_.npcs.wardenVisits == 0 &&
                  !secondGame->worldState_.npcs.wardenPresent &&
                  !secondGame->worldState_.npcs.wardenPending,
              "second context owns independent NPC lifecycle state");
        check(secondGame->worldState_.npcs.firstMeeting[0] &&
                  secondGame->worldState_.npcs.npcPresent[1] &&
                  secondGame->worldState_.npcs.befriendDone[0] == 0 &&
                  secondGame->worldState_.npcs.threatenDone[1] == 0 &&
                  secondGame->worldState_.npcs.interactionCount[2] == 0 &&
                  secondGame->worldState_.npcs.aidPoints[3] == 0 &&
                  secondGame->worldState_.npcs.suspicion[0] == 0 &&
                  secondGame->worldState_.npcs.scrapCount == 0 &&
                  secondGame->worldState_.npcs.gemCount == 0,
              "second context owns independent NPC interaction state");
    }
    {
        check(firstGame->platformContext_ == &firstContext,
              "first game context survives reactivation");
        check(firstGame->worldState_.random == firstGame->rng_.get(),
              "first random binding survives reactivation");
        check(firstGame->worldState_.nextMonsterUid() == 2,
              "first monster UID sequence survives without second-session leakage");
        check(firstGame->gameCanvas_->wantAttack_ &&
                  firstGame->gameCanvas_->messageVisible_ &&
                  firstGame->gameCanvas_->mapGrid17_[0][0] == 7,
              "first canvas runtime state survives without second-session leakage");
        check(firstGame->currentUI_ != nullptr && firstGame->currentUI_->screenId_ == 88 &&
                  firstGame->reloadGame_ && firstGame->helperThreadState_ == 4,
              "first UI and lifecycle state survives without second-session leakage");
        check(firstGame->displayStorage_.getCurrent() == firstDisplayable,
              "first display state survives without second-session leakage");
        check(variantOf(*firstGame).dungeonGen_.campGrid_[0][0] == 7,
              "first camp grid survives without second-session leakage");
        check(firstGame->worldState_.droppedItems.at(0).size() == 1,
              "first dropped-item list survives without second-session leakage");
        check(firstGame->worldState_.chests.at(1).size() == 1,
              "first chest table survives without second-session leakage");
        check(firstGame->worldState_.monsters.at(1).size() == 1,
              "first monster table survives without second-session leakage");
        check(firstGame->worldState_.npcs.wardenVisits == 2 &&
                  firstGame->worldState_.npcs.wardenPresent &&
                  firstGame->worldState_.npcs.wardenPending,
              "first NPC lifecycle state survives without second-session leakage");
        check(!firstGame->worldState_.npcs.firstMeeting[0] &&
                  !firstGame->worldState_.npcs.npcPresent[1] &&
                  firstGame->worldState_.npcs.befriendDone[0] == 1 &&
                  firstGame->worldState_.npcs.threatenDone[1] == 2 &&
                  firstGame->worldState_.npcs.interactionCount[2] == 3 &&
                  firstGame->worldState_.npcs.aidPoints[3] == 4 &&
                  firstGame->worldState_.npcs.suspicion[0] == 5 &&
                  firstGame->worldState_.npcs.scrapCount == 6 &&
                  firstGame->worldState_.npcs.gemCount == 7,
              "first NPC interaction state survives without second-session leakage");
        check(firstGame->dungeons_[0] != secondGame->dungeons_[0],
              "first dungeon registry survives without second-session leakage");
    }

    if (failures == 0) {
        std::cout << "stormhold world state: all checks passed\n";
    }
    return failures == 0 ? 0 : 1;
}
