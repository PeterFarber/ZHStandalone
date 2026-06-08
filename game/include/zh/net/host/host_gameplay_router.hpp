#pragma once

// Listen-host peer lifecycle and inbound ClientInput routing (including delay-B queue).

#include "zh/game/match_world.hpp"
#include "zh/network.hpp"
#include "zh/protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>

namespace zh::net::host {

// Cap per-slot delay queue so a wedged client cannot blow host memory (~few sec @ 60 pps).
inline constexpr std::size_t kMaxDelayedRemoteInputsPerSlot = 512;

struct DelayedRemoteInput {
    std::uint32_t exec_tick{0};
    std::uint32_t seq{0};
    std::uint8_t move_axes{0};
    std::uint8_t ability_axes{0};
    float mouse_x{0.f};
    float mouse_y{0.f};
    std::uint8_t mouse_buttons{0};
};

// Mutable views into AppContext slot tables passed into service_listen.
struct HostPeerSlotTablesMut {
    std::array<bool, kRemoteSlots> &slot_active;
    std::array<void *, kRemoteSlots> &slot_peer;
    std::array<std::uint32_t, kRemoteSlots> &slot_last_seq;
    std::array<std::uint8_t, kRemoteSlots> &slot_move;
    std::array<std::uint8_t, kRemoteSlots> &slot_ability;
    std::array<float, kRemoteSlots> &slot_mouse_x;
    std::array<float, kRemoteSlots> &slot_mouse_y;
    std::array<std::uint8_t, kRemoteSlots> &slot_mouse_buttons;
    std::array<std::deque<DelayedRemoteInput>, kRemoteSlots> &remote_input_schedule;
    std::array<std::uint8_t, kRemoteSlots> &remote_mouse_seen;
    std::uint8_t &occupied_slots_mask;
};

struct HostLobbyRosterMut {
    std::uint8_t &lobby_host_team;
    std::uint8_t &lobby_slot_team_b_mask;
    std::uint8_t &lobby_host_goalie;
    std::uint8_t &lobby_slot_goalie_mask;
};

class HostGameplayRouter {
public:
    void apply_scheduled_inputs(std::uint32_t execute_tick,
                                       std::uint8_t input_delay_ticks_b,
                                       HostPeerSlotTablesMut slots);  // called at start of sim_match

    static void broadcast_lobby_teams(zh::net::ListenHost &host,
                                      HostLobbyRosterMut const &lobby,
                                      std::uint8_t occupied_slots_mask);

    void service_listen(zh::net::ListenHost &host,
                        zh::game::MatchWorld &world,
                        HostPeerSlotTablesMut slots,
                        HostLobbyRosterMut lobby,
                        bool match_running,
                        std::function<void(std::uint8_t slot)> on_guest_disconnected_during_match,
                        std::function<void()> broadcast_lobby_teams,
                        std::uint32_t timeout_ms);
};

}  // namespace zh::net::host
