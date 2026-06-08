// MP3 gameplay cues from resources/sounds/.

#include "zh/ui/game_sounds.hpp"

#include "detail/resource_paths.hpp"

namespace zh::ui {

namespace {

Sound try_load_sound(char const *rel) noexcept {
    char const *path = zh::detail::resolve_resource_file(rel);
    if (path == nullptr) {
        return Sound{};
    }
    Sound const snd = LoadSound(path);
    if (snd.frameCount <= 0) {
        TraceLog(LOG_WARNING, "[zh] LoadSound failed: %s", path);
        return Sound{};
    }
    return snd;
}

void play_if_ready(Sound const &snd, float const volume) noexcept {
    if (snd.frameCount <= 0) {
        return;
    }
    SetSoundVolume(snd, volume);
    PlaySound(snd);
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
    out.ready = out.zoom.frameCount > 0 || out.one_timer.frameCount > 0 || out.shield.frameCount > 0 ||
                out.shot.frameCount > 0 || out.puck_collision.frameCount > 0 ||
                out.puck_metal_collision.frameCount > 0 || out.faceoff_countdown.frameCount > 0;
    if (out.ready) {
        TraceLog(LOG_INFO, "[zh] Loaded gameplay sounds from resources/sounds/");
    } else {
        TraceLog(LOG_WARNING, "[zh] No gameplay sounds found under resources/sounds/");
    }
}

void unload_game_sounds(GameSoundBank &bank) noexcept {
    if (bank.zoom.frameCount > 0) {
        UnloadSound(bank.zoom);
    }
    if (bank.one_timer.frameCount > 0) {
        UnloadSound(bank.one_timer);
    }
    if (bank.shield.frameCount > 0) {
        UnloadSound(bank.shield);
    }
    if (bank.shot.frameCount > 0) {
        UnloadSound(bank.shot);
    }
    if (bank.puck_collision.frameCount > 0) {
        UnloadSound(bank.puck_collision);
    }
    if (bank.puck_metal_collision.frameCount > 0) {
        UnloadSound(bank.puck_metal_collision);
    }
    if (bank.faceoff_countdown.frameCount > 0) {
        UnloadSound(bank.faceoff_countdown);
    }
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
