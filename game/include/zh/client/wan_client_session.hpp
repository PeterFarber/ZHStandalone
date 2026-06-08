#pragma once

// WAN client transport: Welcome/LobbyTeams/Snapshot in, ClientInput out. Predictors live here.

#include "zh/client/client_puck_predictor.hpp"
#include "zh/client/remote_skater_predictor.hpp"
#include "zh/client/sent_input_ring.hpp"
#include "zh/client/snap_ring_buffer.hpp"
#include "zh/network.hpp"
#include "zh/protocol.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace zh::client {

// Mirrors PackedLobbyTeams into AppContext lobby HUD fields.
struct ClientLobbyMirrorMut {
    std::uint8_t &host_team;
    std::uint8_t &slot_team_mask;
    std::uint8_t &occ_mask;
    std::uint8_t &host_goalie;
    std::uint8_t &slot_goalie_mask;
};

// Double-buffered snapshots + wall times; poll_receive rotates and pushes snap_hist.
struct WanSnapDoubleBufferMut {
    zh::PackedSnapshot &snap_prev;
    zh::PackedSnapshot &snap_new;
    bool &have_two_snaps;
    double &snap_prev_time;
    double &snap_new_time;
};

struct WanClientPlayingSendSample {
    std::uint8_t move_axes{0};
    std::uint8_t ability_axes{0};
    float mouse_x{0.f};
    float mouse_y{0.f};
    std::uint8_t mouse_buttons{0};
};

struct WanClientFramePollParams {
    std::uint32_t timeout_ms{0};
    WanSnapDoubleBufferMut snap_bufs;
    ClientLobbyMirrorMut lobby_mirror;
    std::function<double()> wall_time_sec;
    std::function<bool()> is_screen_playing;
    std::function<bool()> is_screen_client_lobby;
    std::function<void()> on_leave_client_lobby_for_snapshot;
    float simulation_fixed_dt_sec{0.f};
    bool send_playing_input{false};
    WanClientPlayingSendSample playing_send{};
    std::function<void(zh::PackedClientInput const &)> on_sent_input_record;
};

class WanClientSession {
public:
    [[nodiscard]] bool connect_ipv4(std::string const &ipv4, std::uint16_t port);

    // Drops GameClient only (same as App::stop_client before this was extracted).
    void disconnect_transport() noexcept;

    // Clears slot/seq/rings/predictors; transport unchanged unless disconnect_transport ran.
    void reset_match_buffers() noexcept;

    static void apply_lobby_teams(zh::PackedLobbyTeams const &lt, ClientLobbyMirrorMut &mirror) noexcept;

    // Dispatches Welcome, LobbyTeams, Snapshot from the client receive queue.
    // Screen predicates stay on App so zh::client does not depend on Screen.
    void poll_receive(std::uint32_t timeout_ms,
                      WanSnapDoubleBufferMut &buffers,
                      ClientLobbyMirrorMut &lobby_mirror,
                      std::function<double()> wall_time_sec,
                      std::function<bool()> is_screen_playing,
                      std::function<bool()> is_screen_client_lobby,
                      std::function<void()> on_leave_client_lobby_for_snapshot,
                      float simulation_fixed_dt_sec);

    void send_gameplay_client_input(zh::PackedClientInput const &payload) noexcept;

    // Playing screen: bump send_seq, optional callback, then send on gameplay channel.
    void send_playing_client_input(std::uint8_t move_axes,
                                   std::uint8_t ability_axes,
                                   float mouse_x,
                                   float mouse_y,
                                   std::uint8_t mouse_buttons,
                                   std::function<void(zh::PackedClientInput const &)> on_built);

    void poll_client_frame(WanClientFramePollParams &params);

    std::unique_ptr<zh::net::GameClient> client{};

    // 255 until PackedWelcome assigns a slot.
    std::uint8_t my_slot{255};
    std::uint32_t send_seq{0};

    ClientSnapRingBuffer snap_hist{};
    ClientSentInputRing sent_input{};
    RemoteSkaterPredictor skater{};
    ClientPuckPredictor puck{};

private:
    void trim_sent_inputs_to_ack_(std::uint32_t ack) noexcept;
};

}  // namespace zh::client
