#pragma once

// Load/play MP3 cues from resources/sounds/ (Raylib audio).

#include "zh/game/match_sound_events.hpp"

#include <raylib.h>

namespace zh::ui {

struct GameSoundBank {
    Sound zoom{};
    Sound one_timer{};
    Sound shield{};
    Sound shot{};
    Sound puck_collision{};
    Sound puck_metal_collision{};
    Sound faceoff_countdown{};
    bool ready{false};
};

void load_game_sounds(GameSoundBank &out) noexcept;
void unload_game_sounds(GameSoundBank &bank) noexcept;

void play_match_sounds(GameSoundBank const &bank, zh::game::MatchSoundEvents const &events) noexcept;

}  // namespace zh::ui
