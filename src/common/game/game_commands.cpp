#include "src/common/game/game.hpp"
#include "src/common/game/ui_widget.hpp"

void Game::commandAction(Command *command, Displayable *displayable) {
    try {
        this->commandAction1(command, displayable);
    } catch (std::exception &throwable) {
        this->displayError(std::string(throwable.what()));
        platform::writeLogLine(std::string("ERROR: unhandled exception handling a UI command: ") +
                               throwable.what());
    } catch (...) {
        this->displayError(std::string("(throwable)"));
        platform::writeLogLine("ERROR: failed to handle a UI command (unknown exception)");
    }
}

void Game::commandAction1(Command *command, Displayable *displayable) {
    if (currentUI_ == nullptr) {
        handleFormCommand(command, displayable);
        return;
    }
    if (command == UIWidget::cmdBack_ || command == UIWidget::cmdCancel_) {
        platformContext_->playSound(platform::Sound::MenuBack);
    } else if (command == UIWidget::cmdSelect_ || command == UIWidget::cmdOk_) {
        platformContext_->playSound(platform::Sound::MenuSelect);
    }

    if (handleNavigationCommand(command)) return;
    if (handleMenuCommand(command)) return;
    if (handleCharacterCommand(command)) return;
    if (handleInventoryCommand(command)) return;
    if (handleAbilitiesCommand(command)) return;
    if (variant_->handleCommand(command)) return;
    if (handleHelpCommand(command)) return;
}
