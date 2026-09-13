#ifndef PLATFORM_SDL_AUDIO_HPP
#define PLATFORM_SDL_AUDIO_HPP

#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "src/common/platform/platform.hpp"

namespace platform {

class SdlAudioSink : public AudioSink {
public:
    SdlAudioSink() = default;
    ~SdlAudioSink() override { shutdown(); }

    SdlAudioSink(const SdlAudioSink &) = delete;
    SdlAudioSink &operator=(const SdlAudioSink &) = delete;

    bool open(const std::string &dir);
    void shutdown();

    void play(Sound sound) override;

private:
    SDL_AudioStream *stream_ = nullptr;
    SDL_AudioSpec spec_{};
    std::map<Sound, std::vector<Uint8>> clips_;

    bool loadClip(const std::string &path, Sound sound);
};

const char *soundFileName(Sound sound);

}

#endif
