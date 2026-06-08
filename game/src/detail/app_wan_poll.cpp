// WAN client side of poll_network: predictors + outbound ClientInput while Playing.
#include "detail/app_context.hpp"

#include "zh/app.hpp"
#include "zh/client/client_interp_frame.hpp"

namespace zh {

using zh::client::ClientInterpFrame;
using zh::client::WanClientFramePollParams;
using zh::client::WanClientPlayingSendSample;
using zh::detail::Screen;

void detail::AppContext::record_sent_client_input(PackedClientInput const &in) {
    wan_.sent_input.push_sent(in, snap_new_.server_tick, snap_new_.input_delay_ticks_b);
}

void detail::AppContext::advance_client_local_predictor(float const frame_dt) {
    if (!remote_client_ || screen_ != Screen::Playing || wan_.my_slot >= kRemoteSlots) {
        return;
    }
    if (!wan_.skater.seeded()) {
        return;
    }

    LocalPlayingInputSample const local = sample_local_playing_input();
    wan_.skater.advance(frame_dt, App::kFixedDt, local.mouse_x, local.mouse_y, local.mouse_buttons,
                        local.ability_axes, wan_.my_slot, snap_new_);
}

void detail::AppContext::advance_client_puck_prediction(float const frame_dt) {
    if (!remote_client_ || screen_ != Screen::Playing || !wan_.skater.seeded()) {
        return;
    }

    ClientInterpFrame r{};
    wan_.snap_hist.sample_interp_frame(GetTime(), stored_client_interp_delay_ms_, snap_new_,
                                          remote_client_, r);

    LocalPlayingInputSample const local = sample_local_playing_input();
    zh::game::MatchSoundEvents const puck_sounds = wan_.puck.advance(
        frame_dt, App::kFixedDt, r, snap_new_, wan_.my_slot, wan_.skater.px(), wan_.skater.py(),
        wan_.skater.vx(), wan_.skater.vy(), wan_.skater.face_x(), wan_.skater.face_y(),
        local.mouse_x, local.mouse_y, local.mouse_buttons, wan_.skater.one_timer_ticks());
    play_match_sounds(puck_sounds);
}

void detail::AppContext::poll_network() {
    service_listen_peers_if_host();

    if (wan_.client == nullptr || !wan_.client->valid()) {
        return;
    }

    WanClientFramePollParams params{
        0U,
        {snap_prev_, snap_new_, have_two_snaps_, snap_prev_time_, snap_new_time_},
        {client_lobby_host_team_, client_lobby_slot_team_mask_, client_lobby_occ_mask_,
         client_lobby_host_goalie_, client_lobby_slot_goalie_mask_},
        []() { return GetTime(); },
        [this]() { return screen_ == Screen::Playing; },
        [this]() { return screen_ == Screen::ClientLobby; },
        [this]() {
            if (screen_ == Screen::ClientLobby) {
                reset_playing_camera();
                screen_ = Screen::Playing;
            }
        },
        App::kFixedDt,
        false,
        {},
        {}};

    if (remote_client_ && screen_ == Screen::Playing) {
        LocalPlayingInputSample const local = sample_local_playing_input();
        params.send_playing_input = true;
        params.playing_send = WanClientPlayingSendSample{
            local.move_axes, local.ability_axes, local.mouse_x, local.mouse_y, local.mouse_buttons};
        params.on_sent_input_record = [this](PackedClientInput const &in) {
            record_sent_client_input(in);
        };
    }

    wan_.poll_client_frame(params);
}

}  // namespace zh
