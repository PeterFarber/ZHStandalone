#include "zh/audio/audio_engine.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace zh::audio {

namespace {

AudioEngine g_engine{};

void trace_audio(char const *fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
    va_end(args);
}

}  // namespace

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() noexcept {
    if (ready_) {
        return true;
    }
    auto *engine = static_cast<ma_engine *>(std::malloc(sizeof(ma_engine)));
    if (engine == nullptr) {
        return false;
    }
    ma_result const r = ma_engine_init(nullptr, engine);
    if (r != MA_SUCCESS) {
        std::free(engine);
        trace_audio("[zh] miniaudio ma_engine_init failed (%d)", static_cast<int>(r));
        return false;
    }
    engine_ = engine;
    ready_ = true;
    trace_audio("[zh] miniaudio engine ready");
    return true;
}

void AudioEngine::shutdown() noexcept {
    if (!ready_ || engine_ == nullptr) {
        ready_ = false;
        engine_ = nullptr;
        return;
    }
    ma_engine_uninit(static_cast<ma_engine *>(engine_));
    std::free(engine_);
    engine_ = nullptr;
    ready_ = false;
}

SoundHandle AudioEngine::load_file(char const *path) noexcept {
    SoundHandle out{};
    if (!ready_ || path == nullptr || path[0] == '\0') {
        return out;
    }
    auto *sound = static_cast<ma_sound *>(std::malloc(sizeof(ma_sound)));
    if (sound == nullptr) {
        return out;
    }
    ma_result const r =
        ma_sound_init_from_file(static_cast<ma_engine *>(engine_), path, 0, nullptr, nullptr, sound);
    if (r != MA_SUCCESS) {
        std::free(sound);
        trace_audio("[zh] miniaudio load failed: %s (%d)", path, static_cast<int>(r));
        return out;
    }
    out.impl = sound;
    return out;
}

void AudioEngine::unload(SoundHandle &sound) noexcept {
    if (!sound.valid()) {
        return;
    }
    ma_sound_uninit(static_cast<ma_sound *>(sound.impl));
    std::free(sound.impl);
    sound.impl = nullptr;
}

void AudioEngine::play(SoundHandle const &sound, float volume) noexcept {
    if (!ready_ || !sound.valid()) {
        return;
    }
    auto *s = static_cast<ma_sound *>(sound.impl);
    ma_sound_set_volume(s, volume);
    ma_sound_seek_to_pcm_frame(s, 0);
    ma_sound_start(s);
}

AudioEngine &global_engine() noexcept {
    return g_engine;
}

}  // namespace zh::audio
