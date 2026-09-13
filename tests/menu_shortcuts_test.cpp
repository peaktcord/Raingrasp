#include <cstdio>

#include "src/common/input/menu_shortcuts.hpp"

namespace {

int failures = 0;
bool inGame = true;
bool optionsOpen = false;
bool menuOpen = false;
bool atRoot = false;
bool selectedIsRoot = true;
int selectedRow = -1;
int closes = 0;

void check(bool value, const char *message) {
    if (!value) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

void pressKey(int32_t code) {
    check(code == 55, "shortcut opens the Options key");
    inGame = false;
    optionsOpen = true;
}

bool isOptionsOpen() { return optionsOpen; }

bool selectRow(int32_t row) {
    selectedRow = row;
    optionsOpen = false;
    menuOpen = selectedIsRoot;
    atRoot = selectedIsRoot;
    return selectedIsRoot;
}

bool isInGame() { return inGame; }

bool isAtShortcutRoot() { return atRoot; }

bool closeShortcutMenu() {
    if (!menuOpen) return false;
    ++closes;
    menuOpen = false;
    atRoot = false;
    inGame = true;
    return true;
}

const shortcuts::Host host = {
    &pressKey, &isOptionsOpen, &selectRow, &isInGame,
    &isAtShortcutRoot, &closeShortcutMenu};
const shortcuts::Binding bindings[] = {{1001, 4, "inventory"}, {1002, 6, "save"}};

void resetFixture() {
    inGame = true;
    optionsOpen = false;
    menuOpen = false;
    atRoot = false;
    selectedIsRoot = true;
    selectedRow = -1;
    closes = 0;
    shortcuts::install(&host, bindings, 2);
}

void finishOpen(int32_t key) {
    check(shortcuts::onKey(key), "shortcut is accepted during gameplay");
    shortcuts::tick();
}

}  // namespace

int main() {
    resetFixture();
    finishOpen(1001);
    check(selectedRow == 4 && menuOpen, "shortcut selects and opens its destination row");
    check(shortcuts::onKey(1001), "repeated root shortcut is consumed");
    check(closes == 1 && inGame, "repeated root shortcut closes to gameplay");

    resetFixture();
    finishOpen(1001);
    atRoot = false;
    check(!shortcuts::onBack() && closes == 0,
          "Back remains one-level navigation below the root");
    atRoot = true;
    check(shortcuts::onBack() && closes == 1 && inGame,
          "Back at a shortcut root closes to gameplay");

    resetFixture();
    selectedIsRoot = false;
    finishOpen(1002);
    check(selectedRow == 6 && !menuOpen, "one-shot action does not become a shortcut root");
    check(!shortcuts::onKey(1002) && closes == 0,
          "repeating a one-shot shortcut does not close an unrelated display");

    resetFixture();
    check(shortcuts::onKey(1002), "first one-shot shortcut is accepted");
    check(shortcuts::onKey(1001), "second shortcut is consumed while the first is pending");
    shortcuts::tick();
    check(selectedRow == 6, "a later shortcut cannot replace a pending quicksave");

    resetFixture();
    finishOpen(1001);
    menuOpen = false;
    inGame = true;
    shortcuts::tick();
    check(shortcuts::onKey(1001) && closes == 0,
          "returning to gameplay clears the old root before reopening");

    return failures ? 1 : 0;
}
