#ifndef COMMON_PLATFORM_PLATFORM_HPP
#define COMMON_PLATFORM_PLATFORM_HPP

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace platform {

using ClockFn = int64_t (*)();
using SleepFn = void (*)(int64_t);
class RenderServices;

inline bool &loggingEnabled() {
    static bool enabled = true;
    return enabled;
}

inline void setLoggingEnabled(bool enabled) {
    loggingEnabled() = enabled;
}

inline std::FILE *&logFile() {
    static std::FILE *file = nullptr;
    return file;
}

inline void setLogFile(const std::string &path) {
    if (logFile() != nullptr) return;
    // One session per file rather than an ever-growing append: a crash report
    // should be the log of the run that crashed.  The previous run is kept
    // alongside it, since a crash is often reported after the game restarts.
    std::remove((path + ".prev").c_str());
    std::rename(path.c_str(), (path + ".prev").c_str());
    logFile() = std::fopen(path.c_str(), "wb");
}

// setLoggingEnabled(false) silences the game's chatter on the console, which
// the shipping build does not want.  The log file is a different audience: it
// exists to be read after a crash, so keep writing to it whenever one is open.
inline void writeLogLine(const std::string &text) {
    if (loggingEnabled()) {
        std::printf("%s\n", text.c_str());
        std::fflush(stdout);
    }
    if (logFile() != nullptr) {
        std::fprintf(logFile(), "%s\n", text.c_str());
        std::fflush(logFile());
    }
}

inline void writeLogLine(const char *text) {
    writeLogLine(std::string(text));
}

// Where a throw came from.  The game catches its own exceptions -- GameCanvas
// tick turns one into an error screen -- so by the time anything logs, the
// frames that would name the culprit are already unwound.  A debug build
// installs a hook here and the throw site calls it *before* throwing, while
// its stack is still standing.  Nothing installs it in a shipping build, so
// the call costs a null test.
using ThrowTraceFn = void (*)(const char *what);

inline ThrowTraceFn &throwTraceHook() {
    static ThrowTraceFn hook = nullptr;
    return hook;
}

inline void setThrowTraceHook(ThrowTraceFn hook) { throwTraceHook() = hook; }

inline void traceThrowSite(const std::string &what) {
    if (ThrowTraceFn hook = throwTraceHook()) hook(what.c_str());
}

inline int64_t wallClockMillis() {
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string normalise(const std::string &name);

class FileLayer {
public:
    virtual ~FileLayer() = default;
    virtual bool read(const std::string &name, std::vector<uint8_t> *out) = 0;
    virtual std::string describe() const = 0;
};

class ResourceStack : public FileLayer {
public:
    void push(FileLayer *layer);
    void clear();
    bool empty() const { return layers_.empty(); }

    bool read(const std::string &name, std::vector<uint8_t> *out) override;
    std::string describe() const override;

private:
    std::vector<FileLayer *> layers_;
};

class SaveStore {
public:
    virtual ~SaveStore() = default;
    virtual bool load(const std::string &name,
                      std::vector<std::vector<uint8_t>> *out) = 0;
    virtual bool save(const std::string &name,
                      const std::vector<std::vector<uint8_t>> &records) = 0;
    virtual bool remove(const std::string &name) = 0;
    virtual std::vector<std::string> list() = 0;
    virtual int64_t lastModified(const std::string &name) = 0;
};

struct PortOptions {
    bool widescreen = false;
    bool fullscreen = false;
    bool letterKeyLabels = true;
    bool moveAutorepeat = false;

    void useHandsetDefaults() {
        widescreen = false;
        letterKeyLabels = false;
        moveAutorepeat = false;
    }
};

enum class Sound {
    MenuMove,
    MenuSelect,
    MenuBack,
    PlayerStep,
    Blocked,
    MeleeHit,
    MeleeMiss,
    PlayerHurt,
    MonsterDied,
    SpellCast,
    SpellFizzle,
    ChestOpened,
    ChestLocked,
    ItemPickedUp,
    LevelUp,
    PlayerDied,
};

class AudioSink {
public:
    virtual ~AudioSink() = default;
    virtual void play(Sound sound) = 0;
};

class PlatformContext {
public:
    PlatformContext() = default;
    PlatformContext(const PlatformContext &) = delete;
    PlatformContext &operator=(const PlatformContext &) = delete;

    void installFileSystem(FileLayer *fs) { fileSystem_ = fs; }
    void installSaveStore(SaveStore *store) { saveStore_ = store; }
    void installClock(ClockFn clock) { clock_ = clock; }
    void installSleep(SleepFn sleep) { sleep_ = sleep; }
    void installRenderServices(RenderServices *renderServices) {
        renderServices_ = renderServices;
    }
    void installAudio(AudioSink *audio) { audio_ = audio; }
    FileLayer *fileSystem() const { return fileSystem_; }
    SaveStore *saveStore() const { return saveStore_; }
    ClockFn clock() const { return clock_; }
    RenderServices *renderServices() const { return renderServices_; }
    AudioSink *audio() const { return audio_; }

    int64_t nowMillis() const {
        return clock_ != nullptr ? clock_() : wallClockMillis();
    }

    bool readResource(const std::string &name, std::vector<uint8_t> *out) const {
        return fileSystem_ != nullptr && fileSystem_->read(normalise(name), out);
    }

    void sleepFor(int64_t milliseconds) const {
        if (sleep_ != nullptr) {
            sleep_(milliseconds);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }

    PortOptions &portOptions() { return portOptions_; }
    const PortOptions &portOptions() const { return portOptions_; }

    using PortOptionsChangedFn = void (*)(void *ctx);
    void installPortOptionsListener(PortOptionsChangedFn fn, void *ctx) {
        portOptionsChanged_ = fn;
        portOptionsCtx_ = ctx;
    }
    void notifyPortOptionsChanged() const {
        if (portOptionsChanged_ != nullptr) portOptionsChanged_(portOptionsCtx_);
    }

    void playSound(Sound sound) const {
        if (audio_ != nullptr) audio_->play(sound);
    }

private:
    FileLayer *fileSystem_ = nullptr;
    SaveStore *saveStore_ = nullptr;
    ClockFn clock_ = nullptr;
    SleepFn sleep_ = nullptr;
    RenderServices *renderServices_ = nullptr;
    AudioSink *audio_ = nullptr;
    PortOptions portOptions_;
    PortOptionsChangedFn portOptionsChanged_ = nullptr;
    void *portOptionsCtx_ = nullptr;
};

PlatformContext *defaultContext();

}

#endif
