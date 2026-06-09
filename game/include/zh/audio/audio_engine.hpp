#pragma once

// Game audio via miniaudio.

namespace zh::audio {

struct SoundHandle {
    void *impl{nullptr};  // ma_sound* (opaque to callers)
    [[nodiscard]] bool valid() const noexcept { return impl != nullptr; }
};

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    AudioEngine(AudioEngine const &) = delete;
    AudioEngine &operator=(AudioEngine const &) = delete;

    [[nodiscard]] bool init() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool ready() const noexcept { return ready_; }

    [[nodiscard]] SoundHandle load_file(char const *path) noexcept;
    void unload(SoundHandle &sound) noexcept;
    void play(SoundHandle const &sound, float volume) noexcept;

private:
    void *engine_{nullptr};  // ma_engine*
    bool ready_{false};
};

[[nodiscard]] AudioEngine &global_engine() noexcept;

}  // namespace zh::audio
