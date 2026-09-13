#include "src/common/game/menulist.hpp"

#include "src/common/game/uistate.hpp"

namespace menulist {

bool moveDown(int32_t entryCount, int32_t lineCount, const SharedArray<int32_t> &lineOf, int32_t *selected,
              int32_t *first, int32_t *last) {
    if (lineOf.isNull()) {
        if (*selected >= entryCount - 1) {
            return false;
        }
        ++*selected;
        if (*last < *selected) {
            ++*first;
            ++*last;
        }
        return true;
    }

    if (*selected + 1 >= lineOf.length()) {
        return false;
    }
    ++*selected;
    int32_t height = *last - *first;
    if (*selected + 1 == lineOf.length()) {
        *first = lineCount - height - 1;
        *last = lineCount - 1;
    } else if (lineOf[*selected + 1] > *last) {
        *last = lineOf[*selected + 1];
        *first = *last - height;
    }
    return true;
}

bool moveUp(const SharedArray<int32_t> &lineOf, int32_t *selected, int32_t *first, int32_t *last) {
    if (*selected <= 0) {
        return false;
    }
    --*selected;
    if (lineOf.isNull()) {
        if (*first > *selected) {
            --*first;
            --*last;
        }
        return true;
    }
    if (lineOf[*selected] < *first) {
        *last -= *first - lineOf[*selected];
        *first = lineOf[*selected];
    }
    return true;
}

int32_t windowBottom(int32_t first, int32_t lineCount, int32_t visibleLines) {
    return first + min32(lineCount, visibleLines) - 1;
}

bool scrollDown(int32_t lineCount, int32_t *first, int32_t *last) {
    if (*last >= lineCount - 1) {
        return false;
    }
    ++*first;
    ++*last;
    return true;
}

bool scrollUp(int32_t *first, int32_t *last) {
    if (*first <= 0) {
        return false;
    }
    --*first;
    --*last;
    return true;
}

namespace {

bool isListShaped(int32_t layout) {
    return layout == uistate::LAYOUT_LIST || layout == uistate::LAYOUT_FORM_1 ||
           layout == uistate::LAYOUT_FORM_2;
}

}

void jump(const NavigationView &screen, int32_t action, int32_t visibleLines, int32_t *first,
          int32_t *last) {
    if (!shortcuts::isJumpKey(action)) {
        return;
    }
    // Repeat the single-step moves rather than recomputing the scroll window.
    // Wrapped rows make the selected entry span a variable number of lines, so
    // moveUp/moveDown are the only places that know how to keep first/last in
    // step with the selection.
    const int32_t step = visibleLines > 1 ? visibleLines - 1 : 1;
    const bool towardEnd = action == shortcuts::JUMP_END || action == shortcuts::JUMP_PAGE_DOWN;
    const bool toExtreme = action == shortcuts::JUMP_HOME || action == shortcuts::JUMP_END;
    bool moved = false;

    if (isListShaped(screen.layout)) {
        int32_t selected = screen.selectedIndex(screen.ctx);
        const int32_t limit = toExtreme ? screen.entryCount(screen.ctx) : step;
        int32_t n = 0;
        while (n < limit) {
            const bool stepped =
                towardEnd ? moveDown(screen.entryCount(screen.ctx), screen.lineCount, screen.lineOf,
                                     &selected, first, last)
                          : moveUp(screen.lineOf, &selected, first, last);
            if (!stepped) break;
            moved = true;
            ++n;
        }
        if (moved) {
            screen.setSelectedIndex(screen.ctx, selected);
        }
    } else if (screen.layout == uistate::LAYOUT_TEXTBOX) {
        const int32_t limit = toExtreme ? screen.lineCount : step;
        int32_t n = 0;
        while (n < limit) {
            const bool stepped = towardEnd ? scrollDown(screen.lineCount, first, last)
                                           : scrollUp(first, last);
            if (!stepped) break;
            moved = true;
            ++n;
        }
    }

    if (moved) {
        screen.repaint(screen.ctx);
    }
}

void navigate(const NavigationView &screen, int32_t action, int32_t *first, int32_t *last) {
    if (action != 1 && action != 6) {
        return;
    }
    bool moved = false;
    if (isListShaped(screen.layout)) {
        int32_t selected = screen.selectedIndex(screen.ctx);
        moved = action == 6
                    ? moveDown(screen.entryCount(screen.ctx), screen.lineCount, screen.lineOf,
                               &selected, first, last)
                    : moveUp(screen.lineOf, &selected, first, last);
        if (moved) {
            screen.setSelectedIndex(screen.ctx, selected);
        }
    } else if (screen.layout == uistate::LAYOUT_TEXTBOX) {
        if (action == 1) {
            screen.selectedIndex(screen.ctx);
        }
        moved = action == 6 ? scrollDown(screen.lineCount, first, last) : scrollUp(first, last);
    }
    if (moved) {
        screen.repaint(screen.ctx);
    }
}

}
