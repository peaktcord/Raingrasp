#ifndef COMMON_GAME_MENULIST_HPP
#define COMMON_GAME_MENULIST_HPP

#include "src/common/input/menu_shortcuts.hpp"
#include "src/common/runtime.hpp"

namespace menulist {

bool moveDown(int32_t entryCount, int32_t lineCount, const SharedArray<int32_t> &lineOf, int32_t *selected,
              int32_t *first, int32_t *last);

bool moveUp(const SharedArray<int32_t> &lineOf, int32_t *selected, int32_t *first, int32_t *last);

int32_t windowBottom(int32_t first, int32_t lineCount, int32_t visibleLines);

constexpr int32_t kListVisibleLines = 10;
constexpr int32_t kFormVisibleLines = 9;

bool scrollDown(int32_t lineCount, int32_t *first, int32_t *last);
bool scrollUp(int32_t *first, int32_t *last);

struct NavigationView {
    int32_t layout = 0;
    int32_t (*selectedIndex)(void *ctx) = nullptr;
    void (*setSelectedIndex)(void *ctx, int32_t index) = nullptr;
    int32_t (*entryCount)(void *ctx) = nullptr;
    void (*repaint)(void *ctx) = nullptr;
    SharedArray<int32_t> lineOf;
    int32_t lineCount = 0;
    void *ctx = nullptr;
};

void navigate(const NavigationView &screen, int32_t action, int32_t *first, int32_t *last);

void jump(const NavigationView &screen, int32_t action, int32_t visibleLines, int32_t *first,
          int32_t *last);

}

#endif
