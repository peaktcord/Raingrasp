#include "src/common/platform/sdl_audio.hpp"

#include <SDL3/SDL_filesystem.h>

namespace platform {

const char *soundFileName(Sound sound) {
    switch (sound) {
        case Sound::MenuMove:     return "MenuMove";
        case Sound::MenuSelect:   return "MenuSelect";
        case Sound::MenuBack:     return "MenuBack";
        case Sound::PlayerStep:   return "PlayerStep";
        case Sound::Blocked:      return "Blocked";
        case Sound::MeleeHit:     return "MeleeHit";
        case Sound::MeleeMiss:    return "MeleeMiss";
        case Sound::PlayerHurt:   return "PlayerHurt";
        case Sound::MonsterDied:  return "MonsterDied";
        case Sound::SpellCast:    return "SpellCast";
        case Sound::SpellFizzle:  return "SpellFizzle";
        case Sound::ChestOpened:  return "ChestOpened";
        case Sound::ChestLocked:  return "ChestLocked";
        case Sound::ItemPickedUp: return "ItemPickedUp";
        case Sound::LevelUp:      return "LevelUp";
        case Sound::PlayerDied:   return "PlayerDied";
    }
    return nullptr;
}

bool SdlAudioSink::open(const std::string &dir) {
    {
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(dir.c_str(), &info) ||
            info.type != SDL_PATHTYPE_DIRECTORY) {
            return false;
        }
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("audio unavailable: %s", SDL_GetError());
        return false;
    }

    spec_.format = SDL_AUDIO_S16;
    spec_.channels = 2;
    spec_.freq = 44100;

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                        &spec_, nullptr, nullptr);
    if (stream_ == nullptr) {
        SDL_Log("no audio device: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);

    static const Sound kAll[] = {
        Sound::MenuMove,    Sound::MenuSelect,   Sound::MenuBack,
        Sound::PlayerStep,  Sound::Blocked,      Sound::MeleeHit,
        Sound::MeleeMiss,   Sound::PlayerHurt,   Sound::MonsterDied,
        Sound::SpellCast,   Sound::SpellFizzle,  Sound::ChestOpened,
        Sound::ChestLocked, Sound::ItemPickedUp, Sound::LevelUp,
        Sound::PlayerDied,
    };
    int loaded = 0;
    for (size_t i = 0; i < sizeof(kAll) / sizeof(kAll[0]); ++i) {
        const char *name = soundFileName(kAll[i]);
        if (name == nullptr) continue;
        if (loadClip(dir + "/" + name + ".wav", kAll[i])) ++loaded;
    }
    if (loaded == 0) {
        shutdown();
        return false;
    }
    SDL_Log("audio: %d/%d clips from %s", loaded,
            (int)(sizeof(kAll) / sizeof(kAll[0])), dir.c_str());
    return true;
}

bool SdlAudioSink::loadClip(const std::string &path, Sound sound) {
    SDL_AudioSpec fileSpec{};
    Uint8 *bytes = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(path.c_str(), &fileSpec, &bytes, &length)) return false;

    Uint8 *converted = nullptr;
    int convertedLength = 0;
    const bool ok = SDL_ConvertAudioSamples(&fileSpec, bytes, (int)length,
                                            &spec_, &converted, &convertedLength);
    SDL_free(bytes);
    if (!ok) {
        SDL_Log("cannot convert %s: %s", path.c_str(), SDL_GetError());
        return false;
    }

    clips_[sound].assign(converted, converted + convertedLength);
    SDL_free(converted);
    return true;
}

void SdlAudioSink::play(Sound sound) {
    if (stream_ == nullptr) return;
    std::map<Sound, std::vector<Uint8>>::const_iterator it = clips_.find(sound);
    if (it == clips_.end() || it->second.empty()) return;

    SDL_PutAudioStreamData(stream_, it->second.data(), (int)it->second.size());
}

void SdlAudioSink::shutdown() {
    if (stream_ != nullptr) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    clips_.clear();
}

}
