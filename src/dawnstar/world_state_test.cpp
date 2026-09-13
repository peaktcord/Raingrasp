#include "src/common/game/game.hpp"
#include "src/dawnstar/profile.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/dawnstar/dungeon_gen.hpp"
#include "src/common/game/game_canvas.hpp"
#include "src/common/game/ui_widget.hpp"

#include <iostream>

using namespace dawnstar;

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

    DungeonGen firstGenerator;
    DungeonGen secondGenerator;
    firstGenerator.buildCampGrid();
    secondGenerator.buildCampGrid();
    firstGenerator.campGrid_[0][0] = 7;
    check(secondGenerator.campGrid_[0][0] == 1,
          "Dawnstar camp-grid construction state does not alias");

    {
        firstGame = new Game(profile(), &firstContext);
        firstGame->setExecutionHooks(firstHelperHook, firstCanvasHook, firstSplashHook);
        firstGame->createGameCanvas();
        check(firstGame->gameCanvas_ == firstGame->gameCanvasStorage_.get(),
              "game exposes its uniquely owned canvas through an observer pointer");
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
        check(firstGame->worldState_.nextMonsterUid() == 1,
              "first session starts its monster UID sequence");
        check(firstGame->worldState_.npcs.firstMeeting[8],
              "Dawnstar initializes all NPCs as unmet");
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
        SharedArray<int8_t> firstChest(8);
        firstChest[0] = 3;
        firstChest[1] = 4;
        firstChest[7] = 1;
        firstGame->worldState_.chests.put(0, firstChest);
        SharedArray<int8_t> replacementChest(8);
        replacementChest[0] = 3;
        replacementChest[1] = 4;
        replacementChest[7] = 2;
        firstGame->worldState_.chests.put(0, replacementChest);
        check(firstGame->worldState_.chests.at(0).size() == 1,
              "coordinate replacement does not append a second chest");
        check(firstGame->worldState_.chests.findAt(0, 3, 4).sameRef(replacementChest),
              "coordinate lookup returns the replacement record");
        SharedArray<int8_t> firstMonster(28);
        firstMonster[1] = 1;
        firstMonster[4] = 6;
        firstMonster[5] = 7;
        firstGame->worldState_.monsters.put(0, firstMonster);
        SharedArray<int8_t> replacementMonster(28);
        replacementMonster[1] = 2;
        replacementMonster[4] = 6;
        replacementMonster[5] = 7;
        firstGame->worldState_.monsters.put(0, replacementMonster);
        firstGame->worldState_.npcs.wardenPending = true;
        firstGame->worldState_.npcs.firstMeeting[5] = false;
        firstGame->worldState_.npcs.befriendDone[0] = 1;
        firstGame->worldState_.npcs.threatenDone[1] = 2;
        firstGame->worldState_.npcs.interactionCount[2] = 3;
        firstGame->worldState_.npcs.aidPoints[3] = 4;
        check(firstGame->worldState_.monsters.at(0).size() == 1,
              "Dawnstar replaces monsters by coordinates");
        check(firstGame->worldState_.monsters.findAt(0, 6, 7).sameRef(replacementMonster),
              "Dawnstar coordinate lookup returns the replacement monster");
        check(firstGame->platformContext_ == &firstContext,
              "first game retains its explicit platform context");
        check(firstGame->worldState_.random == firstGame->rng_.get(),
              "first world state binds its game-owned random generator");
    }
    {
        secondGame = new Game(profile(), &secondContext);
        secondGame->setExecutionHooks(secondHelperHook, secondCanvasHook, secondSplashHook);
        secondGame->createGameCanvas();
        check(secondGame->gameCanvas_ == secondGame->gameCanvasStorage_.get(),
              "second game exposes its own uniquely owned canvas");
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
                  secondGame->gameCanvas_->mapGrid17_[0][0] == 0,
              "second session owns independent canvas runtime state");
        check(secondGame->currentUI_ == nullptr && !secondGame->reloadGame_ &&
                  secondGame->helperThreadState_ == 1,
              "second session owns independent UI and lifecycle state");
        check(&secondGame->displayStorage_ != &firstGame->displayStorage_ &&
                  secondGame->displayStorage_.getCurrent() == nullptr,
              "second session owns an independent display");
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
        check(secondGame->worldState_.chests.at(0).empty(),
              "second context owns an independent chest table");
        check(secondGame->worldState_.monsters.at(0).empty(),
              "second context owns an independent monster table");
        check(!secondGame->worldState_.npcs.wardenPending,
              "second context owns independent NPC lifecycle state");
        check(secondGame->worldState_.npcs.firstMeeting[5] &&
                  secondGame->worldState_.npcs.befriendDone[0] == 0 &&
                  secondGame->worldState_.npcs.threatenDone[1] == 0 &&
                  secondGame->worldState_.npcs.interactionCount[2] == 0 &&
                  secondGame->worldState_.npcs.aidPoints[3] == 0,
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
        check(firstGame->worldState_.droppedItems.at(0).size() == 1,
              "first dropped-item list survives without second-session leakage");
        check(firstGame->worldState_.chests.at(0).size() == 1,
              "first chest table survives without second-session leakage");
        check(firstGame->worldState_.monsters.at(0).size() == 1,
              "first monster table survives without second-session leakage");
        check(firstGame->worldState_.npcs.wardenPending,
              "first NPC lifecycle state survives without second-session leakage");
        check(!firstGame->worldState_.npcs.firstMeeting[5] &&
                  firstGame->worldState_.npcs.befriendDone[0] == 1 &&
                  firstGame->worldState_.npcs.threatenDone[1] == 2 &&
                  firstGame->worldState_.npcs.interactionCount[2] == 3 &&
                  firstGame->worldState_.npcs.aidPoints[3] == 4,
              "first NPC interaction state survives without second-session leakage");
        check(firstGame->dungeons_[0] != secondGame->dungeons_[0],
              "first dungeon registry survives without second-session leakage");
    }

    if (failures == 0) {
        std::cout << "dawnstar world state: all checks passed\n";
    }
    return failures == 0 ? 0 : 1;
}
