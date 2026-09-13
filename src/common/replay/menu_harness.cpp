#include "src/common/replay/menu_harness.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

#include "src/common/game/game.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/runtime.hpp"
#include "src/common/platform/desktop.hpp"
#include "src/common/save_records.hpp"
#include "src/common/render/render.hpp"
#include "src/common/replay/headless.hpp"

namespace menu_harness {
namespace {

struct State {
    int32_t selected;
    int32_t entries;
    int32_t lines;
    int32_t windowTop;
    int32_t windowBottom;
};

constexpr int32_t kKeyUp = -1;
constexpr int32_t kKeyDown = -2;

void row(std::string &out, const std::string &line) {
    out += line;
    out += "\n";
}

std::string i2s(int32_t value) { return std::to_string(value); }

SharedArray<std::string> entries(int32_t count, bool wide) {
    SharedArray<std::string> out(count);
    for (int32_t index = 0; index < count; ++index) {
        out[index] =
            wide ? std::string("An entry far too wide to fit across the screen in one line ") +
                       std::to_string(index)
                 : std::string("Entry ") + std::to_string(index);
    }
    return out;
}

void paint(Game *game, UIWidget *ui) {
    UIWidget *saved = game->currentUI_;
    game->currentUI_ = ui;
    game->uiCanvas_->widget_ = ui;
    ui->requestRepaint();
    ui->flushRepaints();
    game->currentUI_ = saved;
}

void setup(UIWidget *ui, const SharedArray<std::string> &rows, bool form) {
    if (form) {
        ui->setupForm(std::string("Fixture"), std::string("Body text"), rows);
    } else {
        ui->setupList(std::string("Fixture"), rows, true);
    }
}

State state(const game::Profile &profile, UIWidget *ui) {
    if (profile.wrapFormRows) {
        int32_t entryCount = ui->rowToOption_.isNull() ? 0 : ui->rowToOption_.length();
        return {ui->selectedIndex(), entryCount, ui->lineCount_, ui->windowTop_,
                ui->windowBottom_};
    }
    int32_t rows = ui->rowCount();
    return {ui->selectedIndex(), rows, rows, ui->windowTop_, ui->windowBottom_};
}

void dumpState(std::string &out, const game::Profile &profile, const char *fixture,
               const char *step, UIWidget *ui) {
    State s = state(profile, ui);
    row(out, std::string("state\t") + fixture + "\t" + step + "\t" + i2s(s.selected) + "\t" +
                 i2s(s.entries) + "\t" + i2s(s.lines) + "\t" + i2s(s.windowTop) + "\t" +
                 i2s(s.windowBottom));
}

void runFixture(std::string &out, const game::Profile &profile, Game *game, const char *name,
                int32_t count, bool wide, bool form) {
    UIWidget *ui = new UIWidget(game, form ? 5 : 3, 900);
    setup(ui, entries(count, wide), form);
    paint(game, ui);
    dumpState(out, profile, name, "setup", ui);

    int32_t steps = count + 2;
    for (int32_t index = 0; index < steps; ++index) {
        ui->keyPressed(kKeyDown);
        paint(game, ui);
        dumpState(out, profile, name, ("down" + i2s(index + 1)).c_str(), ui);
    }
    for (int32_t index = 0; index < steps; ++index) {
        ui->keyPressed(kKeyUp);
        paint(game, ui);
        dumpState(out, profile, name, ("up" + i2s(index + 1)).c_str(), ui);
    }

    Command *select = ui->positiveCommand();
    Command *cancel = ui->negativeCommand();
    row(out, std::string("softkeys\t") + name + "\t" +
                 (select != nullptr ? select->getLabel() : "(none)") + "\t" +
                 (cancel != nullptr ? cancel->getLabel() : "(none)"));
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
        std::printf("menu states match the baseline (%zu bytes)\n", got.size());
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

    std::printf("FAIL: menu navigation changed\n");
    for (size_t index = 0;
         index < actualLines.size() || index < expectedLines.size(); ++index) {
        const std::string &actual =
            index < actualLines.size() ? actualLines[index] : std::string();
        const std::string &expectedLine =
            index < expectedLines.size() ? expectedLines[index] : std::string();
        if (actual != expectedLine) {
            std::printf("  first difference at line %zu\n", index + 1);
            std::printf("    expected: %s\n", expectedLine.c_str());
            std::printf("    actual:   %s\n", actual.c_str());
            break;
        }
    }
    std::printf("  %zu lines expected, %zu produced\n", expectedLines.size(),
                actualLines.size());
    return 1;
}

}

int run(int argc, char **argv, const game::Profile &profile) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <resource-dir> <output.tsv>\n", argv[0]);
        return 2;
    }

    Resources::setRoot(argv[1]);
    SaveRecordFiles::setRoot("saves/rms-menudump");
    render::Surface screen(176, 208);
    render::Context renderer(&screen);
    platform::defaultContext()->installRenderServices(&renderer);
    profile.initStatics(platform::defaultContext());

    Game *game = new Game(profile, platform::defaultContext());
    game->setExecutionHooks(headless::runJobInline<Game>);
    game->startApplication();
    if (game->splashUI_ == nullptr || game->splashUI_->progressPercent_ < 100) {
        std::fprintf(stderr, "FAIL: appload did not complete\n");
        return 1;
    }

    std::string out;
    row(out,
        "# columns: state<TAB>fixture<TAB>step<TAB>selected<TAB>entries<TAB>lines<TAB>window0<TAB>window1");
    runFixture(out, profile, game, "list_three", 3, false, false);
    runFixture(out, profile, game, "list_twenty", 20, false, false);
    runFixture(out, profile, game, "list_one", 1, false, false);
    runFixture(out, profile, game, "list_empty", 0, false, false);
    runFixture(out, profile, game, "list_wide", 1, true, false);
    runFixture(out, profile, game, "form_three", 3, false, true);
    runFixture(out, profile, game, "form_twenty", 20, false, true);
    runFixture(out, profile, game, "form_wide", 1, true, true);

    int status = emit(out, argv[2], argv[3]);
    std::fflush(stdout);
    std::_Exit(status);
}

}
