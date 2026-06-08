// Host/client session reset and lobby team broadcast helpers.

#include "detail/app_context.hpp"

#include "zh/game/spawn_layout.hpp"

#include "zh/game/constants.hpp"

namespace zh {

using namespace ::zh::game;

// Full host lobby reset: MatchWorld, slot tables, roster masks, delay B from settings.
void detail::AppContext::host_reset_session() {
    match_running_ = false;
    reset_playing_camera();
    world_.reset_session(static_cast<float>(GetScreenWidth()),
                         static_cast<float>(GetScreenHeight()));
    world_.input_delay_ticks_b = stored_remote_delay_ticks_;

    occupied_slots_mask_ = 0;

    lobby_host_team_ = 0;
    lobby_slot_team_b_mask_ = 0;
    lobby_host_goalie_ = 0;
    lobby_slot_goalie_mask_ = 0;
    host_session_hint_[0] = '\0';
    host_session_hint_until_sec_ = 0.;

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        remote_mouse_seen_[i] = 0;
        slot_active_[i] = false;
        slot_peer_[i] = nullptr;
        slot_last_seq_[i] = 0;
        slot_move_[i] = 0;
        slot_ability_[i] = 0;
        slot_mouse_x_[i] = 0.f;
        slot_mouse_y_[i] = 0.f;
        slot_mouse_buttons_[i] = 0;
    }

    for (auto &dq : remote_input_schedule_) {
        dq.clear();
    }
}

// WAN client disconnect cleanup: snap buffers and WanClientSession predictors.
void detail::AppContext::client_reset_session() {
    remote_client_ = false;
    reset_playing_camera();
    snap_prev_ = {};
    snap_new_ = {};
    have_two_snaps_ = false;
    snap_prev_time_ = 0.;
    snap_new_time_ = 0.;
    wan_.reset_match_buffers();
    client_lobby_host_team_ = 0;
    client_lobby_slot_team_mask_ = 0;
    client_lobby_occ_mask_ = 0;
    client_lobby_host_goalie_ = 0;
    client_lobby_slot_goalie_mask_ = 0;
}

void detail::AppContext::broadcast_lobby_teams_if_hosting() {
    if (listen_server_ == nullptr || !listen_server_->valid()) {
        return;
    }
    zh::net::host::HostLobbyRosterMut lobby{lobby_host_team_, lobby_slot_team_b_mask_,
                                            lobby_host_goalie_, lobby_slot_goalie_mask_};
    host_router_.broadcast_lobby_teams(*listen_server_, lobby, occupied_slots_mask_);
}

// Copy lobby roster picks into MatchWorld spawn positions before match start.
void detail::AppContext::apply_match_start_spawn_from_lobby_teams() {
    apply_roster_spawns_to_match(world_,
                                 lobby_host_team_,
                                 lobby_host_goalie_,
                                 lobby_slot_team_b_mask_,
                                 lobby_slot_goalie_mask_,
                                 slot_active_);
}

}  // namespace zh
