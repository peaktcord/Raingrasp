#include "src/common/game/ui_widget.hpp"
#include "src/common/game/textwrap.hpp"

Font *UIWidget::softKeyFont_ = nullptr;
Font *UIWidget::titleFont_ = nullptr;
Font *UIWidget::bodyFont_ = nullptr;
Font *UIWidget::progressFont_ = nullptr;
Command *UIWidget::cmdOk_ = nullptr;
Command *UIWidget::cmdSelect_ = nullptr;
Command *UIWidget::cmdCancel_ = nullptr;
Command *UIWidget::cmdBack_ = nullptr;
Command *UIWidget::cmdExit_ = nullptr;
std::vector<std::string> UIWidget::copyrightLines_;

void UIWidget::initializeStatics() {
    softKeyFont_ = Font::getFont(0, 1, 8);
    titleFont_ = Font::getFont(0, 1, 0);
    bodyFont_ = Font::getFont(0, 1, 8);
    progressFont_ = Font::getFont(0, 1, 0);
    cmdOk_ = new Command(std::string("Ok"), 3, 0);
    cmdSelect_ = new Command(std::string("Select"), 3, 0);
    cmdCancel_ = new Command(std::string("Cancel"), 4, 0);
    cmdBack_ = new Command(std::string("Back"), 4, 0);
    cmdExit_ = new Command(std::string("Exit"), 7, 0);
    copyrightLines_ = {std::string("(c) 2003 Vir2L Studios, "), std::string("a ZeniMax Media company. "),
                       std::string("The Elder Scrolls and Vir2L "), std::string("are registered trademarks "),
                       std::string("of ZeniMax Media Inc. "), std::string("All rights reserved.")};
}

UIWidget::UIWidget(game::UiHost *game, int32_t n, int32_t n2)
    : game_(game), profile_(&game->profile()), canvas_(game->uiCanvas()) {
    this->layout_ = n;
    this->screenId_ = n2;
    this->contextIndex_ = 0;
    this->backTarget_ = nullptr;
    this->nextTarget_ = nullptr;
    this->tagTemplate_ = std::string();
    this->commands_.reserve(5);
    this->rows_.setNull();
    this->rowToOption_.setNull();
    this->prompts_[0].reset();
    this->prompts_[1].reset();
    if (n == uistate::LAYOUT_TEXTBOX) {
        this->a(cmdOk_);
        this->a(game_->commandListener());
    }
}

void UIWidget::setScreenId(int32_t n) {
    this->screenId_ = n;
}

void UIWidget::setupList(const std::string &string, SharedArray<std::string> stringArray, bool bl) {
    this->title_ = string;
    this->rows_ = stringArray;
    this->rowToOption_.setNull();
    this->marginLeft_ = 15;
    this->wrapMarginLeft_ = 15;
    this->marginRight_ = 15;
    this->font_ = bodyFont_;
    this->windowTop_ = 0;
    this->selected_ = 0;
    this->lineCount_ = this->rowCount();
    this->windowBottom_ =
        menulist::windowBottom(this->windowTop_, this->lineCount_, menulist::kListVisibleLines);
    this->a(cmdSelect_);
    if (bl) {
        this->a(cmdCancel_);
    }
    this->a(game_->commandListener());
}

void UIWidget::setupTextBox(const std::string &string, const std::string &string2) {
    this->rows_.setNull();
    this->rowToOption_.setNull();
    this->title_ = string;
    this->prompts_[0] = string2;
    this->prompts_[1].reset();
    this->marginLeft_ = 5;
    this->wrapMarginLeft_ = profile_->textBoxWrapMargin;
    this->marginRight_ = profile_->textBoxWrapMargin;
    this->windowTop_ = 0;
    this->font_ = bodyFont_;
    this->rewrapTextBox();
}

void UIWidget::rewrapTextBox() {
    this->rows_ = this->wrapText(this->prompts_[0].value_or(""));
    this->lineCount_ = this->rows_.length();
    const int32_t visibleLines = min32(this->lineCount_, 11);
    this->windowTop_ = max32(0, min32(this->windowTop_, this->lineCount_ - visibleLines));
    this->windowBottom_ = menulist::windowBottom(this->windowTop_, this->lineCount_, 11);
}

void UIWidget::setupExitScreen() {
    std::string string1("");
    for (const std::string &line : copyrightLines_) string1 = string1 + line;
    this->setupTextBox(std::string("Exiting"), string1);
    this->b(cmdOk_);
    this->a(cmdExit_);
}

void UIWidget::setupForm(const std::string &string, const std::string &string2, SharedArray<std::string> stringArray) {
    this->title_ = string;
    this->prompts_[0] = string2;
    this->prompts_[1].reset();
    this->marginLeft_ = profile_->formMargin;
    this->wrapMarginLeft_ = profile_->formMargin;
    this->marginRight_ = profile_->formMargin;
    this->font_ = bodyFont_;
    this->rows_ = stringArray;
    this->windowTop_ = 0;
    this->selected_ = 0;
    this->lineCount_ = this->rowCount();
    if (profile_->wrapFormRows) {
        this->rowToOption_ = SharedArray<int32_t>(this->lineCount_);
        int32_t n1 = 0;
        while (n1 < this->lineCount_) {
            this->rowToOption_[n1] = n1;
            n1 = (int16_t)(n1 + 1);
        }
        SharedArray<std::string> stringArray1;
        int32_t n2 = 0;
        int32_t n3 = 0;
        while (n3 < this->lineCount_) {
            ++n2;
            if (this->font_->stringWidth(this->rows_[n3]) >
                this->canvasWidth() - this->marginLeft_ - this->marginRight_) {
                SharedArray<std::string> stringArray2 = this->wrapText(this->rows_[n3]);
                int32_t n4 = stringArray2.length();
                stringArray1 = SharedArray<std::string>(this->lineCount_ + n4 - 1);
                copySharedArray(this->rows_, 0, stringArray1, 0, n3);
                copySharedArray(stringArray2, 0, stringArray1, n3, n4);
                copySharedArray(this->rows_, n3 + 1, stringArray1, n3 + 1 + --n4,
                                 this->lineCount_ - n3 - 1);
                int32_t n5 = this->rowToOption_.length() - 1;
                while (n5 >= n2) {
                    int32_t n6 = n5--;
                    this->rowToOption_[n6] = this->rowToOption_[n6] + n4;
                }
                this->lineCount_ += n4;
                n3 = (int16_t)(n3 + n4);
                this->rows_ = SharedArray<std::string>(this->lineCount_);
                copySharedArray(stringArray1, 0, this->rows_, 0, this->lineCount_);
            }
            n3 = (int16_t)(n3 + 1);
        }
    } else {
        this->rowToOption_.setNull();
    }
    this->windowBottom_ =
        menulist::windowBottom(this->windowTop_, this->lineCount_, menulist::kFormVisibleLines);
    this->a(cmdSelect_);
    this->a(cmdCancel_);
    this->a(game_->commandListener());
    if (string2.find("<TAG>") != std::string::npos) {
        this->tagTemplate_ = std::string(string2);
    }
}

void UIWidget::setupFormWithSubtitle(const std::string &string, const std::string &string2, const std::string &string3,
                                     SharedArray<std::string> stringArray) {
    this->setupForm(string, string2, stringArray);
    this->prompts_[1] = string3;
}

void UIWidget::onShow() {
    switch (this->layout_) {
        case uistate::LAYOUT_SPLASH: {
            this->startThread();
            break;
        }
    }
}

void UIWidget::onHide() {
    switch (this->layout_) {
        case uistate::LAYOUT_DOWNLOAD:
        case uistate::LAYOUT_SPLASH: {
            this->stopThread();
            break;
        }
    }
}

void UIWidget::paint(Graphics *graphics) {
    switch (this->layout_) {
        case uistate::LAYOUT_DOWNLOAD: {
            break;
        }
        case uistate::LAYOUT_SPLASH: {
            this->drawSplash(graphics);
            break;
        }
        case uistate::LAYOUT_LIST: {
            this->drawList(graphics);
            break;
        }
        case uistate::LAYOUT_FORM_1: {
            this->drawForm(graphics, 1);
            break;
        }
        case uistate::LAYOUT_FORM_2: {
            this->drawForm(graphics, 2);
            break;
        }
        case uistate::LAYOUT_TEXTBOX: {
            this->drawTextBox(graphics);
            break;
        }
        case uistate::LAYOUT_PROGRESS_NEW_GAME:
        case uistate::LAYOUT_PROGRESS_LOAD_GAME:
        case uistate::LAYOUT_PROGRESS_SAVE_GAME:
        case uistate::LAYOUT_PROGRESS_LOAD_DUNGEON: {
            this->drawProgress(graphics);
            break;
        }
    }
    this->drawSoftKeys(graphics);
}

void UIWidget::drawSplash(Graphics *graphics) {
    game::SplashArt art = game_->splashArt();
    graphics->setColor(0);
    graphics->fillRect(0, 0, this->canvasWidth(), 20 + this->canvasHeight());
    if (game_->showingCarrierLogo()) {
        graphics->setColor(0xFFFFFF);
        graphics->fillRect(0, 0, this->canvasWidth(), 20 + this->canvasHeight());
        graphics->drawImage(art.publisherLogo, this->canvasWidth() / 2, 10, 17);
        int32_t n1 = 10 + art.publisherLogo->getHeight() + 3;
        graphics->setColor(0);
        for (const std::string &line : copyrightLines_) {
            graphics->drawString(line, this->canvasWidth() / 2, n1, 17);
            n1 += 14;
        }
        graphics->drawString(std::string("Distributed by:"), this->canvasWidth() / 2, 143, 17);
        graphics->drawImage(art.carrierLogo, this->canvasWidth() / 2, 158, 17);
        return;
    }
    graphics->setColor(profile_->splashBackground);
    graphics->fillRect(0, 0, this->canvasWidth(), 20 + this->canvasHeight());
    graphics->drawImage(art.top, this->canvasWidth() / 2, profile_->splashTopY, 17);
    graphics->drawImage(art.bottom, this->canvasWidth() / 2, profile_->splashBottomY, 17);
    if (!game_->showingSplash()) {
        graphics->setColor(0xFFFFFF);
        graphics->fillRect(12, 165, 152, 22);
        graphics->setColor(0xA00000);
        graphics->fillRect(13, 166, 3 * this->progressPercent_ / 2, 20);
    }
}

void UIWidget::drawProgress(Graphics *graphics) {
    menupaint::drawProgressDialog(graphics, this->canvasWidth(), this->canvasHeight(), progressFont_,
                                  this->layout_, this->progressPercent_, profile_->menuBackground);
}

void UIWidget::drawTitleBar(Graphics *graphics) {
    menupaint::drawTitleBar(graphics, this->canvasWidth(), titleFont_, this->title_);
}

void UIWidget::drawList(Graphics *graphics) {
    graphics->setColor(profile_->menuBackground);
    graphics->fillRect(0, 0, this->canvasWidth(), 20 + this->canvasHeight());
    this->drawTitleBar(graphics);
    graphics->setFont(this->font_);
    this->lineHeight_ = this->font_->getHeight();
    this->drawY_ = 20;
    this->drawRows(graphics);
    menupaint::drawScrollArrows(graphics, this->windowTop_, this->windowBottom_, this->lineCount_);
}

void UIWidget::drawForm(Graphics *graphics, int32_t n) {
    graphics->setColor(profile_->menuBackground);
    graphics->fillRect(0, 0, this->canvasWidth(), 20 + this->canvasHeight());
    this->drawTitleBar(graphics);
    graphics->setFont(this->font_);
    this->lineHeight_ = this->font_->getHeight();
    this->drawY_ = 20;
    graphics->setColor(0xFFFF00);
    int32_t n2 = 0;
    while (n2 < n) {
        const std::optional<std::string> &prompt = this->prompts_[n2];
        if (prompt) {
            graphics->setColor(n2 == 0 ? 0xFFFF00 : 0xFFFFFF);
            if (profile_->formPromptsAlwaysWrap || this->wrapBody_) {
                SharedArray<std::string> lines = this->wrapText(*prompt);
                int32_t n1 = 0;
                while (n1 < lines.length()) {
                    graphics->drawString(lines[n1], this->marginLeft_, this->drawY_, 20);
                    this->drawY_ += this->lineHeight_;
                    ++n1;
                }
            } else {
                graphics->drawString(*prompt, this->marginLeft_, this->drawY_, 20);
                this->drawY_ += this->lineHeight_;
            }
        }
        ++n2;
    }
    this->drawY_ += 5;
    this->drawRows(graphics);
    menupaint::drawScrollArrows(graphics, this->windowTop_, this->windowBottom_, this->lineCount_);
}

void UIWidget::drawRows(Graphics *graphics) {
    int32_t selectedLine = this->selectedIndex();
    int32_t selectedSpan = 1;
    if (!this->rowToOption_.isNull()) {
        selectedLine = this->rowToOption_[this->selected_];
        selectedSpan = this->selected_ + 1 != this->rowToOption_.length()
                           ? this->rowToOption_[this->selected_ + 1] - selectedLine
                           : this->lineCount_ - selectedLine;
    }
    menupaint::drawRows(graphics, this->rows_, this->windowTop_, this->windowBottom_, selectedLine,
                        selectedSpan, this->marginLeft_, this->canvasWidth(), this->lineHeight_,
                        &this->drawY_);
}

void UIWidget::drawTextBox(Graphics *graphics) {
    graphics->setColor(profile_->menuBackground);
    graphics->fillRect(0, 0, this->canvasWidth(), 20 + this->canvasHeight());
    this->drawTitleBar(graphics);
    graphics->setFont(this->font_);
    this->lineHeight_ = this->font_->getHeight();
    this->drawY_ = 20;
    graphics->setColor(0xFFFF00);
    int32_t n1 = this->windowTop_;
    while (n1 <= this->windowBottom_) {
        graphics->drawString(this->rows_[n1], this->marginLeft_, this->drawY_, 20);
        this->drawY_ += this->lineHeight_;
        ++n1;
    }
    menupaint::drawScrollArrows(graphics, this->windowTop_, this->windowBottom_, this->lineCount_);
}

SharedArray<std::string> UIWidget::wrapText(const std::string &string) {
    int32_t n1 = this->canvasWidth() - this->wrapMarginLeft_ - this->marginRight_;
    std::vector<std::string> lines = textwrap::wrap(profile_->textWrap, this->font_, n1, string);
    SharedArray<std::string> wrapped((int32_t)lines.size());
    for (int32_t line = 0; line < wrapped.length(); ++line) {
        wrapped[line] = lines[(size_t)line];
    }
    return wrapped;
}

namespace {

int32_t readSelected(void *ctx) { return ((UIWidget *)ctx)->selectedIndex(); }
void writeSelected(void *ctx, int32_t index) { ((UIWidget *)ctx)->selected_ = index; }
int32_t readEntryCount(void *ctx) { return ((UIWidget *)ctx)->rowCount(); }
void repaintAfterMove(void *ctx) { ((UIWidget *)ctx)->afterListMove(); }

}

void UIWidget::keyPressed(int32_t n) {
    Command *command;
    if (n == -6) {
        command = this->negativeCommand();
        if (command != nullptr) {
            this->listener_->commandAction(command, this->canvas_);
            return;
        }
    } else if (n == -7 && (command = this->positiveCommand()) != nullptr) {
        this->listener_->commandAction(command, this->canvas_);
        return;
    }
    const int32_t action = this->gameAction(n);
    if ((action == 2 || action == 5) && this->selectedIndex() >= 0 &&
        this->selectedIndex() < this->horizontalValueRows_ &&
        (command = this->positiveCommand()) != nullptr) {
        this->listener_->commandAction(command, this->canvas_);
        return;
    }
    menulist::NavigationView screen;
    screen.layout = this->layout_;
    screen.selectedIndex = readSelected;
    screen.setSelectedIndex = writeSelected;
    screen.entryCount = readEntryCount;
    screen.repaint = repaintAfterMove;
    screen.lineOf = this->rowToOption_;
    screen.lineCount = this->lineCount_;
    screen.ctx = this;
    if (shortcuts::isJumpKey(n)) {
        menulist::jump(screen, n, this->windowBottom_ - this->windowTop_ + 1, &this->windowTop_,
                       &this->windowBottom_);
        return;
    }
    menulist::navigate(screen, action, &this->windowTop_, &this->windowBottom_);
}

void UIWidget::afterListMove() {
    this->requestRepaint();
    this->flushRepaints();
}

SharedArray<std::string> UIWidget::rowLabels() {
    switch (this->layout_) {
        case uistate::LAYOUT_LIST:
        case uistate::LAYOUT_FORM_1:
        case uistate::LAYOUT_FORM_2: {
            return this->rows_;
        }
    }
    return SharedArray<std::string>();
}

int32_t UIWidget::rowCount() {
    if (this->layout_ == uistate::LAYOUT_LIST || this->layout_ == uistate::LAYOUT_FORM_1 ||
        this->layout_ == uistate::LAYOUT_FORM_2) {
        return this->rows_.isNull() ? 0 : this->rows_.length();
    }
    return 0;
}

std::string UIWidget::selectedText() {
    return this->rows_[this->selectedIndex()];
}

int32_t UIWidget::selectedIndex() {
    switch (this->layout_) {
        case uistate::LAYOUT_LIST:
        case uistate::LAYOUT_FORM_1:
        case uistate::LAYOUT_FORM_2: {
            if (profile_->emptyListSelectsNone && this->rowCount() == 0) {
                return -1;
            }
            return this->selected_;
        }
    }
    return -1;
}

void UIWidget::setSelectedIndex(int32_t n) {
    switch (this->layout_) {
        case uistate::LAYOUT_LIST:
        case uistate::LAYOUT_FORM_1:
        case uistate::LAYOUT_FORM_2: {
            this->selected_ = n;
            if (this->selected_ >= this->rows_.length()) {
                this->selected_ = this->rows_.length() - 1;
            }
            if (this->selected_ > this->windowBottom_) {
                int32_t n1 = this->selected_ - this->windowBottom_;
                this->windowBottom_ += n1;
                this->windowTop_ += n1;
                break;
            }
            if (this->selected_ >= this->windowTop_) break;
            int32_t n2 = this->windowTop_ - this->selected_;
            this->windowBottom_ -= n2;
            this->windowTop_ -= n2;
        }
    }
}

void UIWidget::setPromptText(int32_t n, const std::string &string) {
    if (n == 0 && (this->layout_ == uistate::LAYOUT_FORM_1 || this->layout_ == uistate::LAYOUT_FORM_2 ||
                   this->layout_ == uistate::LAYOUT_TEXTBOX)) {
        this->prompts_[0] = string;
        if (this->layout_ == uistate::LAYOUT_TEXTBOX) {
            this->rewrapTextBox();
        }
    } else if (n == 1 && this->layout_ == uistate::LAYOUT_FORM_2) {
        this->prompts_[1] = string;
    }
}

void UIWidget::setTitle(const std::string &string) {
    this->title_ = string;
}

void UIWidget::setBodyText(const std::string &string) {
    if (this->layout_ == uistate::LAYOUT_FORM_1 || this->layout_ == uistate::LAYOUT_FORM_2) {
        this->prompts_[0] = string;
        if (profile_->bodyTextResetsScroll) {
            this->windowTop_ = 0;
        }
    } else if (this->layout_ == uistate::LAYOUT_TEXTBOX) {
        this->prompts_[0] = string;
        if (profile_->bodyTextResetsScroll) {
            this->windowTop_ = 0;
        }
        this->rewrapTextBox();
    }
}

std::string UIWidget::bodyText() {
    if (this->layout_ == uistate::LAYOUT_FORM_1 || this->layout_ == uistate::LAYOUT_FORM_2 ||
        this->layout_ == uistate::LAYOUT_TEXTBOX) {
        return this->prompts_[0].value_or("");
    }
    return std::string();
}

void UIWidget::a(Command *command) {
    this->commands_.push_back(command);
}

void UIWidget::setRowLabels(SharedArray<std::string> rows) {
    if (this->layout_ != uistate::LAYOUT_LIST || rows.length() != this->rowCount()) {
        return;
    }
    this->rows_ = rows;
    this->requestRepaint();
    this->flushRepaints();
}

void UIWidget::b(Command *command) {
    for (auto entry = this->commands_.begin(); entry != this->commands_.end(); ++entry) {
        if (*entry == command) {
            this->commands_.erase(entry);
            return;
        }
    }
}

void UIWidget::a(CommandListener *commandListener) {
    this->listener_ = commandListener;
}

void UIWidget::drawSoftKeys(Graphics *graphics) {
    menupaint::drawSoftKeys(graphics, this->canvasWidth(), softKeyFont_, this->commands_,
                            this->negativeCommand(), this->positiveCommand(),
                            profile_->softKeyNegativeY, profile_->softKeyPositiveY);
}

Command *UIWidget::positiveCommand() {
    return menupaint::positiveCommand(this->commands_, cmdOk_, cmdSelect_);
}

Command *UIWidget::negativeCommand() {
    return menupaint::negativeCommand(this->commands_, cmdBack_, cmdCancel_);
}

void UIWidget::requestRepaint() {
    UIWidget *current = game_->currentUi();
    if (current != nullptr && this->screenId_ != current->screenId_) {
        return;
    }
    this->canvas_->repaint();
}

void UIWidget::flushRepaints() {
    this->canvas_->serviceRepaints();
}

int32_t UIWidget::canvasWidth() {
    return this->canvas_->getWidth();
}

int32_t UIWidget::canvasHeight() {
    return this->canvas_->getHeight();
}

int32_t UIWidget::gameAction(int32_t n) {
    return this->canvas_->getGameAction(n);
}

void UIWidget::startThread() {
    this->threadRunning_ = true;
    game_->splashStarted(this);
}

void UIWidget::stopThread() {
    this->threadRunning_ = false;
}

namespace {

void splashRepaint(void *ctx, splashphase::Phase) {
    UIWidget *self = (UIWidget *)ctx;
    self->requestRepaint();
    self->flushRepaints();
}

void splashCarrierLogo(void *ctx) {
    UIWidget *self = (UIWidget *)ctx;
    self->game_->setShowingCarrierLogo(true);
    self->game_->setShowingSplash(false);
}

void splashPublisherLogo(void *ctx) {
    UIWidget *self = (UIWidget *)ctx;
    self->game_->setShowingSplash(true);
    self->game_->setShowingCarrierLogo(false);
}

void splashFinish(void *ctx) {
    UIWidget *self = (UIWidget *)ctx;
    self->game_->setShowingSplash(false);
    // Normally the publisher-logo hook has already cleared this. A skip can
    // finish straight out of the carrier logo without passing through it, so
    // clear it here too rather than leave the flag set against released art.
    self->game_->setShowingCarrierLogo(false);
    self->game_->releaseSplashArt();
    self->game_->showDisplayable(self->nextTarget_);
}

}

splashphase::Hooks UIWidget::splashHooks() {
    splashphase::Hooks hooks;
    hooks.repaint = splashRepaint;
    hooks.showCarrierLogo = splashCarrierLogo;
    hooks.showPublisherLogo = splashPublisherLogo;
    hooks.finish = splashFinish;
    hooks.ctx = this;
    return hooks;
}

bool UIWidget::splashStep(int64_t dtMs) {
    try {
        return splashphase::step(&this->splash_, this->splashHooks(), dtMs,
                                 this->threadRunning_, this->progressPercent_);
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to advance the splash screen: ") +
                               exception.what());
        game_->showDisplayable(game_->errorForm());
        this->splash_.phase = splashphase::Phase::Done;
        return false;
    } catch (...) {
        platform::writeLogLine("ERROR: unhandled exception advancing the splash screen");
        game_->showDisplayable(game_->errorForm());
        this->splash_.phase = splashphase::Phase::Done;
        return false;
    }
}

bool UIWidget::splashSkip() {
    try {
        return splashphase::skip(&this->splash_, this->splashHooks());
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to skip the splash screen: ") +
                               exception.what());
        game_->showDisplayable(game_->errorForm());
        this->splash_.phase = splashphase::Phase::Done;
        return false;
    } catch (...) {
        platform::writeLogLine("ERROR: unhandled exception skipping the splash screen");
        game_->showDisplayable(game_->errorForm());
        this->splash_.phase = splashphase::Phase::Done;
        return false;
    }
}
