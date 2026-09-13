#include "src/common/game/registered_application.hpp"

#include <cstdlib>

Command *RegisteredApplication::cmdExit_ = nullptr;
Command *RegisteredApplication::cmdFatalExit_ = nullptr;
Command *RegisteredApplication::cmdFinalExit_ = nullptr;

void RegisteredApplication::initializeStatics() {
    cmdExit_ = new Command(std::string("Exit"), 7, 1);
    cmdFatalExit_ = new Command(std::string("Exit"), 7, 1);
    cmdFinalExit_ = new Command(std::string("Exit"), 7, 1);
}

void RegisteredApplication::fatalErrorAlert(const std::string &string) {
    Form *form = new Form(std::string("Fatal Error!"), {new StringItem(std::string(), string)});
    form->addCommand(cmdFinalExit_);
    form->setCommandListener(this);
    this->display_->setCurrent(form);
}

void RegisteredApplication::finalExit() {
    destroyApplication(true);
    std::exit(0);
}

void RegisteredApplication::commandAction(Command *command, Displayable *) {
    if (command == cmdExit_) {
        this->exit();
    }
}
