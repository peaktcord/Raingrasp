#ifndef COMMON_GAME_REGISTERED_APPLICATION_HPP
#define COMMON_GAME_REGISTERED_APPLICATION_HPP

#include "src/common/runtime.hpp"
#include "src/common/ui.hpp"

class RegisteredApplication : public CommandListener {
public:
    bool verifyLicence_ = true;
    std::string appName_;
    Display displayStorage_;
    Display *display_ = nullptr;
    static Command *cmdExit_;
    static Command *cmdFatalExit_;
    static Command *cmdFinalExit_;

    RegisteredApplication() : display_(&displayStorage_) {}
    void startApplication() { startRegisteredApplication(); }
    virtual void startRegisteredApplication() = 0;
    virtual void pauseApplication() {}
    virtual void destroyApplication(bool) {}
    void exit() { finalExit(); }
    void finalExit();
    void errorAlert(const std::string &) {}
    void fatalErrorAlert(const std::string &string);
    void commandAction(Command *command, Displayable *displayable) override;
    static void initializeStatics();
};

#endif
