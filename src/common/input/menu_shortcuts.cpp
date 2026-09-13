#include "src/common/input/menu_shortcuts.hpp"

namespace shortcuts {

namespace {

const Host *g_host = nullptr;
const Binding *g_bindings = nullptr;
int g_count = 0;

const int32_t kOptionsKey = 55;

const int kTimeoutTicks = 120;

int32_t g_row = -1;
int32_t g_key = 0;
int32_t g_activeKey = 0;
bool g_opened = false;
int g_ticks = 0;

void resetPending() {
    g_row = -1;
    g_key = 0;
    g_opened = false;
    g_ticks = 0;
}

void reset() {
    resetPending();
    g_activeKey = 0;
}

}

void install(const Host *host, const Binding *bindings, int count) {
    g_host = host;
    g_bindings = bindings;
    g_count = count;
    reset();
}

bool onKey(int32_t key) {
    if (g_host == nullptr || g_bindings == nullptr) {
        return false;
    }
    for (int n = 0; n < g_count; ++n) {
        if (g_bindings[n].key != key) {
            continue;
        }
        // Do not let a second shortcut replace one that has already started.
        // Opening Options is asynchronous on the handset-style canvas, so a
        // quick F6/F9 sequence could previously turn a pending save into a
        // load before the save action was ever selected.
        if (g_row >= 0) {
            return true;
        }
        if (g_activeKey == key && g_host->inGame != nullptr && !g_host->inGame() &&
            g_host->closeShortcutMenu != nullptr && g_host->closeShortcutMenu()) {
            reset();
            return true;
        }
        if (g_host->inGame == nullptr || !g_host->inGame()) {
            return false;
        }
        g_row = g_bindings[n].row;
        g_key = key;
        g_opened = false;
        g_ticks = 0;
        return true;
    }
    return false;
}

bool onBack() {
    if (g_activeKey == 0 || g_host == nullptr || g_host->atShortcutRoot == nullptr ||
        !g_host->atShortcutRoot() || g_host->closeShortcutMenu == nullptr ||
        !g_host->closeShortcutMenu()) {
        return false;
    }
    reset();
    return true;
}

void tick() {
    if (g_host == nullptr) {
        return;
    }
    if (g_activeKey != 0 && g_host->inGame != nullptr && g_host->inGame()) {
        g_activeKey = 0;
    }
    if (g_row < 0) return;
    if (g_host->optionsOpen != nullptr && g_host->optionsOpen()) {
        if (g_host->selectRow != nullptr) {
            if (g_host->selectRow(g_row)) g_activeKey = g_key;
        }
        resetPending();
        return;
    }
    if (!g_opened) {
        if (g_host->pressKey != nullptr) {
            g_host->pressKey(kOptionsKey);
        }
        g_opened = true;
    }
    // Desktop hosts may open Options synchronously. Select the requested row
    // in the same frame so one-shot actions such as quicksave cannot linger
    // as replaceable pending input until the next gameplay tick.
    if (g_host->optionsOpen != nullptr && g_host->optionsOpen()) {
        if (g_host->selectRow != nullptr) {
            if (g_host->selectRow(g_row)) g_activeKey = g_key;
        }
        resetPending();
        return;
    }
    if (++g_ticks > kTimeoutTicks) {
        reset();
    }
}

bool pending() { return g_row >= 0; }

void cancel() { reset(); }

}
