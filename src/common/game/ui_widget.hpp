#ifndef COMMON_GAME_UI_WIDGET_HPP
#define COMMON_GAME_UI_WIDGET_HPP

#include <optional>
#include <vector>

#include "src/common/game/commandflow.hpp"
#include "src/common/game/menuaction.hpp"
#include "src/common/game/menulist.hpp"
#include "src/common/game/menupaint.hpp"
#include "src/common/game/profile.hpp"
#include "src/common/game/splashphase.hpp"
#include "src/common/game/ui_host.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/widget_canvas.hpp"
#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

class UIWidget : public game::Widget {
public:
    std::vector<Command *> commands_;
    CommandListener *listener_ = nullptr;
    static Font *softKeyFont_;
    static Font *titleFont_;
    static Font *bodyFont_;
    static Font *progressFont_;
    static Command *cmdOk_;
    static Command *cmdSelect_;
    static Command *cmdCancel_;
    static Command *cmdBack_;
    static Command *cmdExit_;
    static std::vector<std::string> copyrightLines_;
    int32_t layout_ = 0;
    int32_t screenId_ = 0;
    commandflow::ItemRows itemActions_;
    int32_t contextIndex_ = 0;
    game::UiHost *game_ = nullptr;
    const game::Profile *profile_ = nullptr;
    game::WidgetCanvas *canvas_ = nullptr;
    std::string tagTemplate_;
    game::DisplayTarget backTarget_;
    game::DisplayTarget nextTarget_;
    int32_t lineHeight_ = 0;
    int32_t drawY_ = 0;
    Font *font_ = nullptr;
    int32_t marginLeft_ = 0;
    int32_t wrapMarginLeft_ = 0;
    int32_t marginRight_ = 0;
    int32_t windowTop_ = 0;
    int32_t windowBottom_ = 0;
    int32_t lineCount_ = 0;
    std::string title_;
    std::optional<std::string> prompts_[2];
    bool wrapBody_ = false;
    SharedArray<std::string> rows_;
    SharedArray<int32_t> rowToOption_;
    int32_t selected_ = 0;
    int32_t horizontalValueRows_ = 0;
    int32_t progressPercent_ = 0;
    bool threadRunning_ = false;
    splashphase::State splash_;

    UIWidget(game::UiHost *game, int32_t layout, int32_t screenId);
    ~UIWidget() override = default;

    void setScreenId(int32_t n);
    void bindCanvas() {}
    void bindCanvasAgain() {}

    void setupList(const std::string &title, SharedArray<std::string> rows, bool cancellable);
    void setupTextBox(const std::string &title, const std::string &text);
    void setupExitScreen();
    void setupForm(const std::string &title, const std::string &body, SharedArray<std::string> rows);
    void setupFormWithSubtitle(const std::string &title, const std::string &body, const std::string &subtitle,
                               SharedArray<std::string> rows);

    void onShow();
    void onHide();
    void paint(Graphics *graphics) override;
    void drawSplash(Graphics *graphics);
    void drawProgress(Graphics *graphics);
    void drawTitleBar(Graphics *graphics);
    void drawList(Graphics *graphics);
    void drawForm(Graphics *graphics, int32_t prompts);
    void drawRows(Graphics *graphics);
    void drawTextBox(Graphics *graphics);
    SharedArray<std::string> wrapText(const std::string &string);

    void keyPressed(int32_t n) override;
    void afterListMove();
    SharedArray<std::string> rowLabels();
    int32_t rowCount();
    std::string selectedText();
    int32_t selectedIndex();
    void setSelectedIndex(int32_t n);
    void setRowLabels(SharedArray<std::string> rows);
    void setHorizontalValueRows(int32_t count) { horizontalValueRows_ = count; }
    void setPromptText(int32_t n, const std::string &string);
    void setTitle(const std::string &string);
    void setBodyText(const std::string &string);
    std::string bodyText();

    void a(Command *command);
    void b(Command *command);
    void a(CommandListener *commandListener);
    void addCommand(Command *command) override { this->a(command); }
    void removeCommand(Command *command) override { this->b(command); }
    void setCommandListener(CommandListener *listener) override { this->a(listener); }
    void drawSoftKeys(Graphics *graphics);
    Command *positiveCommand();
    Command *negativeCommand();

    void requestRepaint();
    void flushRepaints();
    int32_t canvasWidth();
    int32_t canvasHeight();
    int32_t gameAction(int32_t n);

    void startThread();
    void stopThread();
    splashphase::Hooks splashHooks();
    bool splashStep(int64_t dtMs);
    bool splashSkip();

    static void initializeStatics();

private:
    void rewrapTextBox();
};

#endif
