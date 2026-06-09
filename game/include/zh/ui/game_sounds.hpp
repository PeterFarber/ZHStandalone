#pragma once

// Gameplay MP3 cues via miniaudio (resources/sounds/).

#include "zh/audio/audio_engine.hpp"
#include "zh/game/match_sound_events.hpp"

namespace zh::ui {

struct GameSoundBank {
    zh::audio::SoundHandle zoom{};
    zh::audio::SoundHandle one_timer{};
    zh::audio::SoundHandle shield{};
    zh::audio::SoundHandle shot{};
    zh::audio::SoundHandle puck_collision{};
    zh::audio::SoundHandle puck_metal_collision{};
    zh::audio::SoundHandle faceoff_countdown{};
    bool ready{false};
};

void load_game_sounds(GameSoundBank &out) noexcept;
void unload_game_sounds(GameSoundBank &bank) noexcept;

void play_match_sounds(GameSoundBank const &bank, zh::game::MatchSoundEvents const &events) noexcept;

}  // namespace zh::ui
