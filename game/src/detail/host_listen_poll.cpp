// Host ENet pump each frame — connect/disconnect, ClientInput, lobby broadcasts.
#include "detail/app_context.hpp"

#include "zh/net/host/host_gameplay_router.hpp"

#include <cstdio>

namespace zh::detail {

void AppContext::service_listen_peers_if_host() {
    if (listen_server_ == nullptr || !listen_server_->valid()) {
        return;
    }

    zh::net::host::HostPeerSlotTablesMut slots{
        slot_active_,       slot_peer_,         slot_last_seq_,     slot_move_,
        slot_ability_,      slot_mouse_x_,      slot_mouse_y_,      slot_mouse_buttons_,
        remote_input_schedule_, remote_mouse_seen_, occupied_slots_mask_};

    zh::net::host::HostLobbyRosterMut lobby{lobby_host_team_, lobby_slot_team_b_mask_,
                                            lobby_host_goalie_, lobby_slot_goalie_mask_};

    host_router_.service_listen(
        *listen_server_, world_, slots, lobby, match_running_,
        [this](std::uint8_t const slot) {
            (void)std::snprintf(host_session_hint_, sizeof(host_session_hint_),
                                "Guest disconnected — freed slot %u.",
                                static_cast<unsigned>(slot));
            host_session_hint_until_sec_ = GetTime() + 4.5;
        },
        [this]() { broadcast_lobby_teams_if_hosting(); }, 0U);
}

}  // namespace zh::detail
