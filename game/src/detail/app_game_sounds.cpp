// Gameplay sound bank load/unload and playback helpers.

#include "detail/app_context.hpp"

#include "zh/gfx/gfx_compat.hpp"
#include "zh/ui/game_sounds.hpp"

namespace zh::detail {

void AppContext::load_game_sound_bank() {
    unload_game_sound_bank_if_any();
    zh::ui::load_game_sounds(game_sounds_);
}

void AppContext::unload_game_sound_bank_if_any() {
    zh::ui::unload_game_sounds(game_sounds_);
}

void AppContext::play_match_sounds(zh::game::MatchSoundEvents const &events) noexcept {
    if (screen_ != Screen::Playing) {
        return;
    }

    zh::game::MatchSoundEvents filtered = events;
    double const now = GetTime();
    if (filtered.puck_collision) {
        if (now - last_puck_collision_sound_sec_ < 0.12) {
            filtered.puck_collision = false;
        } else {
            last_puck_collision_sound_sec_ = now;
        }
    }
    if (filtered.puck_metal_collision) {
        if (now - last_puck_metal_collision_sound_sec_ < 0.12) {
            filtered.puck_metal_collision = false;
        } else {
            last_puck_metal_collision_sound_sec_ = now;
        }
    }

    zh::ui::play_match_sounds(game_sounds_, filtered);
}

void AppContext::poll_match_ambient_sounds() noexcept {
    if (screen_ != Screen::Playing) {
        sound_prev_faceoff_countdown_secs_ = 0;
        return;
    }

    std::uint8_t phase = 255;
    std::uint32_t faceoff_ticks = 0U;
    if (remote_client_) {
        phase = snap_new_.match_phase;
        faceoff_ticks = snap_new_.faceoff_countdown_ticks;
    } else if (listen_server_ != nullptr && listen_server_->valid() && match_running_) {
        phase = static_cast<std::uint8_t>(world_.phase);
        faceoff_ticks = world_.faceoff_countdown_ticks;
    } else {
        sound_prev_faceoff_countdown_secs_ = 0;
        return;
    }

    if (phase != static_cast<std::uint8_t>(zh::game::MatchPhase::FaceoffCountdown) ||
        faceoff_ticks == 0U) {
        sound_prev_faceoff_countdown_secs_ = 0;
        return;
    }

    int const countdown_secs = zh::game::faceoff_countdown_secs_from_ticks(faceoff_ticks);
    if (countdown_secs > 0 && countdown_secs != sound_prev_faceoff_countdown_secs_) {
        zh::game::MatchSoundEvents events{};
        events.faceoff_countdown = true;
        play_match_sounds(events);
        sound_prev_faceoff_countdown_secs_ = countdown_secs;
    }
}

void AppContext::poll_client_local_sounds() noexcept {
    if (screen_ != Screen::Playing) {
        client_sound_prev_boost_os_ = 0;
        client_sound_prev_one_timer_ = 0;
        client_sound_prev_shield_ = 0;
        host_sound_prev_one_timer_ = 0;
        host_sound_prev_shield_ = 0;
        return;
    }

    zh::game::MatchSoundEvents events{};

    if (remote_client_) {
        if (!wan_.skater.seeded()) {
            client_sound_prev_boost_os_ = 0;
            client_sound_prev_one_timer_ = 0;
            client_sound_prev_shield_ = 0;
            return;
        }

        std::uint32_t const boost_os = wan_.skater.boost_overspeed_ticks();
        if (boost_os > 0U && client_sound_prev_boost_os_ == 0U) {
            events.boost = true;
        }
        client_sound_prev_boost_os_ = boost_os;

        std::uint32_t const one_timer = wan_.skater.one_timer_ticks();
        if (one_timer > 0U && client_sound_prev_one_timer_ == 0U) {
            events.one_timer = true;
        }
        client_sound_prev_one_timer_ = one_timer;

        if (wan_.skater.is_goalie()) {
            std::uint32_t const shield = wan_.skater.shield_ticks();
            if (shield > 0U && client_sound_prev_shield_ == 0U) {
                events.shield = true;
            }
            client_sound_prev_shield_ = shield;
        } else {
            client_sound_prev_shield_ = 0;
        }
    } else if (listen_server_ != nullptr && listen_server_->valid() && match_running_) {
        std::uint32_t const one_timer = world_.host_one_timer_ticks;
        if (one_timer > 0U && host_sound_prev_one_timer_ == 0U) {
            events.one_timer = true;
        }
        host_sound_prev_one_timer_ = one_timer;

        if ((world_.match_host_goalie & 1U) != 0U) {
            std::uint32_t const shield = world_.host_shield_ticks;
            if (shield > 0U && host_sound_prev_shield_ == 0U) {
                events.shield = true;
            }
            host_sound_prev_shield_ = shield;
        } else {
            host_sound_prev_shield_ = 0;
        }
    } else {
        host_sound_prev_one_timer_ = 0;
        host_sound_prev_shield_ = 0;
    }

    play_match_sounds(events);
}

}  // namespace zh::detail
