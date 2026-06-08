// WAN client: connect, poll snapshots, send gameplay input, run local predictors.

#include "zh/client/wan_client_session.hpp"

namespace zh::client {

bool WanClientSession::connect_ipv4(std::string const &ipv4, std::uint16_t const port) {
    disconnect_transport();
    client = std::make_unique<zh::net::GameClient>();
    if (!client || !client->valid()) {
        client.reset();
        return false;
    }
    if (!client->connect_ipv4(ipv4, port)) {
        client.reset();
        return false;
    }
    return true;
}

void WanClientSession::disconnect_transport() noexcept {
    client.reset();
}

void WanClientSession::reset_match_buffers() noexcept {
    my_slot = 255;
    send_seq = 0;
    snap_hist.clear();
    sent_input.clear();
    skater.reset();
    puck.reset();
}

void WanClientSession::apply_lobby_teams(zh::PackedLobbyTeams const &lt,
                                         ClientLobbyMirrorMut &mirror) noexcept {
    mirror.host_team = lt.host_team & 1u;
    mirror.slot_team_mask = lt.slot_team_mask;
    mirror.occ_mask = lt.slot_occupied_mask;
    mirror.host_goalie = lt.host_goalie & 1u;
    mirror.slot_goalie_mask = lt.slot_goalie_mask;
}

void WanClientSession::send_gameplay_client_input(zh::PackedClientInput const &payload) noexcept {
    if (!client || !client->valid() || !client->has_server_peer()) {
        return;
    }
    client->send_packet({reinterpret_cast<std::byte const *>(&payload), sizeof(payload)},
                         zh::kChannelGameplay, static_cast<enet_uint32>(0));
}

void WanClientSession::send_playing_client_input(
    std::uint8_t const move_axes,
    std::uint8_t const ability_axes,
    float const mouse_x,
    float const mouse_y,
    std::uint8_t const mouse_buttons,
    std::function<void(zh::PackedClientInput const &)> on_built) {
    if (!client || !client->valid() || !client->has_server_peer()) {
        return;
    }

    zh::PackedClientInput in{};
    in.id = zh::NetMsg::ClientInput;
    in.seq = ++send_seq;
    in.move_axes = move_axes;
    in.ability_axes = ability_axes;
    in.mouse_x = mouse_x;
    in.mouse_y = mouse_y;
    in.mouse_buttons = mouse_buttons;
    if (on_built) {
        on_built(in);
    }
    send_gameplay_client_input(in);
}

// One frame: drain receive queue, optionally send playing input.
void WanClientSession::poll_client_frame(WanClientFramePollParams &params) {
    poll_receive(params.timeout_ms, params.snap_bufs, params.lobby_mirror, params.wall_time_sec,
                 params.is_screen_playing, params.is_screen_client_lobby,
                 params.on_leave_client_lobby_for_snapshot, params.simulation_fixed_dt_sec);

    if (!params.send_playing_input || !client || !client->valid() || !client->has_server_peer()) {
        return;
    }

    WanClientPlayingSendSample const &s = params.playing_send;
    send_playing_client_input(s.move_axes, s.ability_axes, s.mouse_x, s.mouse_y, s.mouse_buttons,
                                params.on_sent_input_record);
}

// Trim sent-input ring to host ack; feed follow axes into skater predictor.
void WanClientSession::trim_sent_inputs_to_ack_(std::uint32_t const ack) noexcept {
    auto const follow_axes = sent_input.trim_to_ack(ack);
    if (follow_axes) {
        skater.apply_ack_follow_axes(*follow_axes);
    }
}

void WanClientSession::poll_receive(std::uint32_t const timeout_ms,
                                    WanSnapDoubleBufferMut &b,
                                    ClientLobbyMirrorMut &lobby_mirror,
                                    std::function<double()> wall_time_sec,
                                    std::function<bool()> is_screen_playing,
                                    std::function<bool()> is_screen_client_lobby,
                                    std::function<void()> on_leave_client_lobby_for_snapshot,
                                    float const simulation_fixed_dt_sec) {
    if (!client || !client->valid()) {
        return;
    }

    client->service(
        [&](ENetEvent const &ev) {
            if (ev.type != ENET_EVENT_TYPE_RECEIVE || ev.packet == nullptr) {
                return;
            }
            // Welcome -> my_slot; LobbyTeams -> roster mirror; Snapshot -> snap ring + predictors.
            auto *const data = reinterpret_cast<std::uint8_t const *>(ev.packet->data);
            std::size_t const len = ev.packet->dataLength;
            if (len < 1U) {
                return;
            }
            auto const tag = static_cast<zh::NetMsg>(data[0]);
            if (tag == zh::NetMsg::Welcome) {
                zh::PackedWelcome w{};
                if (zh::try_copy_net_message(data, len, w)) {
                    my_slot = w.slot_id;
                }
                return;
            }
            if (tag == zh::NetMsg::LobbyTeams) {
                zh::PackedLobbyTeams lt{};
                if (zh::try_copy_net_message(data, len, lt)) {
                    apply_lobby_teams(lt, lobby_mirror);
                }
                return;
            }
            if (tag == zh::NetMsg::Snapshot) {
                zh::PackedSnapshot snap{};
                if (!zh::try_copy_net_message(data, len, snap)) {
                    return;
                }
                // First snapshot while still on client lobby screen -> transition to playing.
                if (is_screen_client_lobby()) {
                    on_leave_client_lobby_for_snapshot();
                }
                b.snap_prev = b.snap_new;
                b.snap_prev_time = b.snap_new_time;
                b.snap_new = snap;
                b.snap_new_time = wall_time_sec();
                b.have_two_snaps = true;
                snap_hist.push(b.snap_new, b.snap_new_time);

                if (is_screen_playing() && my_slot < zh::kRemoteSlots) {
                    trim_sent_inputs_to_ack_(snap.ack_seq[my_slot]);
                    std::uint8_t const s = my_slot;
                    zh::PackedSnapshot const &nw = b.snap_new;
                    zh::PackedSnapshot const &prv = b.snap_prev;
                    // Authority snap + replay unacked inputs for local skater/puck.
                    skater.authority_resync(s, nw, prv, simulation_fixed_dt_sec);
                    puck.authority_resync(nw);
                    skater.replay_from_hist(sent_input, true, simulation_fixed_dt_sec);
                }
            }
        },
        timeout_ms);
}

}  // namespace zh::client
