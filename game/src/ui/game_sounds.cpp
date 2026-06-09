#include "zh/ui/game_sounds.hpp"

#include "detail/resource_paths.hpp"

#include <cstdio>

namespace zh::ui {

namespace {

zh::audio::SoundHandle try_load_sound(char const *rel) noexcept {
    char const *path = zh::detail::resolve_resource_file(rel);
    if (path == nullptr) {
        return {};
    }
    return zh::audio::global_engine().load_file(path);
}

void play_if_ready(zh::audio::SoundHandle const &snd, float const volume) noexcept {
    if (!snd.valid()) {
        return;
    }
    zh::audio::global_engine().play(snd, volume);
}

}  // namespace

void load_game_sounds(GameSoundBank &out) noexcept {
    unload_game_sounds(out);
    out.zoom = try_load_sound("sounds/Zoom.mp3");
    out.one_timer = try_load_sound("sounds/OneTimer.mp3");
    out.shield = try_load_sound("sounds/Shield.mp3");
    out.shot = try_load_sound("sounds/Shot.mp3");
    out.puck_collision = try_load_sound("sounds/PuckCollision.mp3");
    out.puck_metal_collision = try_load_sound("sounds/PuckMetalCollision.mp3");
    out.faceoff_countdown = try_load_sound("sounds/Countdown.mp3");
    out.ready = out.zoom.valid() || out.one_timer.valid() || out.shield.valid() ||
                out.shot.valid() || out.puck_collision.valid() ||
                out.puck_metal_collision.valid() || out.faceoff_countdown.valid();
    if (out.ready) {
        std::fprintf(stderr, "[zh] Loaded gameplay sounds from resources/sounds/\n");
    } else {
        std::fprintf(stderr, "[zh] No gameplay sounds found under resources/sounds/\n");
    }
}

void unload_game_sounds(GameSoundBank &bank) noexcept {
    auto &eng = zh::audio::global_engine();
    eng.unload(bank.zoom);
    eng.unload(bank.one_timer);
    eng.unload(bank.shield);
    eng.unload(bank.shot);
    eng.unload(bank.puck_collision);
    eng.unload(bank.puck_metal_collision);
    eng.unload(bank.faceoff_countdown);
    bank = GameSoundBank{};
}

void play_match_sounds(GameSoundBank const &bank, zh::game::MatchSoundEvents const &events) noexcept {
    if (!bank.ready) {
        return;
    }
    if (events.boost) {
        play_if_ready(bank.zoom, 0.85f);
    }
    if (events.one_timer) {
        play_if_ready(bank.one_timer, 0.9f);
    }
    if (events.shield) {
        play_if_ready(bank.shield, 0.92f);
    }
    if (events.shot) {
        play_if_ready(bank.shot, 0.95f);
    }
    if (events.puck_collision) {
        play_if_ready(bank.puck_collision, 0.7f);
    }
    if (events.puck_metal_collision) {
        play_if_ready(bank.puck_metal_collision, 0.75f);
    }
    if (events.faceoff_countdown) {
        play_if_ready(bank.faceoff_countdown, 0.88f);
    }
}

}  // namespace zh::ui
