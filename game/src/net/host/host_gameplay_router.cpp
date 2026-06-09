// ENet connect/disconnect/receive for the listen host; assigns slots and queues delayed input.

#include "zh/net/host/host_gameplay_router.hpp"

#include "zh/game/map_runtime.hpp"

#include <cstdio>

namespace zh::net::host {

namespace {

// peer->data holds slot index + 1 (0 means untagged).
[[nodiscard]] std::size_t host_slot_from_peer_data(ENetPeer const *peer) {
    if (peer == nullptr || peer->data == nullptr) {
        return kRemoteSlots;
    }
    auto const tag = reinterpret_cast<std::uintptr_t>(peer->data);
    if (tag == 0U || tag > kRemoteSlots) {
        return kRemoteSlots;
    }
    return static_cast<std::size_t>(tag - 1U);
}

void host_tag_peer_slot(ENetPeer *peer, std::size_t slot) {
    peer->data = reinterpret_cast<void *>(static_cast<std::uintptr_t>(slot + 1U));
}

void host_clear_peer_tag(ENetPeer *peer) {
    if (peer != nullptr) {
        peer->data = nullptr;
    }
}

}  // namespace

// Pop due entries from each slot's delay-B queue into live slot_* fields.
void HostGameplayRouter::apply_scheduled_inputs(std::uint32_t const execute_tick,
                                                std::uint8_t const input_delay_ticks_b,
                                                HostPeerSlotTablesMut const slots) {
    if (input_delay_ticks_b == 0) {
        return;
    }
    for (std::size_t s = 0; s < kRemoteSlots; ++s) {
        auto &q = slots.remote_input_schedule[s];
        if (!slots.slot_active[s]) {
            q.clear();
            continue;
        }
        while ((!q.empty()) && q.front().exec_tick < execute_tick) {
            q.pop_front();
        }
        DelayedRemoteInput applied{};
        bool have = false;
        while ((!q.empty()) && q.front().exec_tick == execute_tick) {
            applied = q.front();
            q.pop_front();
            have = true;
        }
        if (have) {
            slots.slot_last_seq[s] = applied.seq;
            slots.slot_move[s] = applied.move_axes;
            slots.slot_ability[s] = applied.ability_axes;
            slots.slot_mouse_x[s] = applied.mouse_x;
            slots.slot_mouse_y[s] = applied.mouse_y;
            slots.slot_mouse_buttons[s] = applied.mouse_buttons;
        }
    }
}

void HostGameplayRouter::broadcast_lobby_teams(zh::net::ListenHost &host,
                                             HostLobbyRosterMut const &lobby,
                                             std::uint8_t const occupied_slots_mask) {
    if (!host.valid()) {
        return;
    }
    zh::PackedLobbyTeams lt{};
    lt.id = zh::NetMsg::LobbyTeams;
    lt.host_team = lobby.lobby_host_team;
    lt.slot_team_mask = lobby.lobby_slot_team_b_mask;
    lt.slot_occupied_mask = occupied_slots_mask;
    lt.host_goalie = lobby.lobby_host_goalie;
    lt.slot_goalie_mask = lobby.lobby_slot_goalie_mask;
    host.broadcast_packet({reinterpret_cast<std::byte const *>(&lt), sizeof(lt)}, zh::kChannelReliable,
                          ENET_PACKET_FLAG_RELIABLE);
}

// Assign slot on connect, tear down on disconnect, route ClientInput (immediate or delay-B queue).
void HostGameplayRouter::service_listen(zh::net::ListenHost &host,
                                        zh::game::MatchWorld &world,
                                        HostPeerSlotTablesMut const slots,
                                        HostLobbyRosterMut const lobby,
                                        bool const match_running,
                                        std::function<void(std::uint8_t slot)> on_guest_disconnected_during_match,
                                        std::function<void()> broadcast_lobby_teams,
                                        std::uint32_t const timeout_ms) {
    if (!host.valid()) {
        return;
    }
    using namespace zh::game;

    host.service(
        [&](ENetEvent const &ev) {
            if (ev.type == ENET_EVENT_TYPE_CONNECT && ev.peer != nullptr) {
                std::size_t slot = kRemoteSlots;
                for (std::size_t i = 0; i < kRemoteSlots; ++i) {
                    if (!slots.slot_active[i]) {
                        slot = i;
                        break;
                    }
                }
                if (slot == kRemoteSlots) {
                    enet_peer_disconnect(ev.peer, 0);
                    return;
                }
                slots.slot_active[slot] = true;
                slots.slot_peer[slot] = ev.peer;
                host_tag_peer_slot(ev.peer, slot);

                MapPoint const slot_spawn = zh::game::map_runtime().slot_spawn_or_default(slot);
                world.slot_sk_x[slot] = slot_spawn.x;
                world.slot_sk_y[slot] = slot_spawn.y;
                world.slot_has_waypoint[slot] = false;
                slots.remote_mouse_seen[slot] = 0;
                world.slot_sk_vx[slot] = 0.f;
                world.slot_sk_vy[slot] = 0.f;
                world.slot_boost_active_ticks[slot] = 0;
                world.slot_boost_cd_ticks[slot] = {};
                world.slot_boost_overspeed_ticks[slot] = 0;
                world.slot_slide_cd_ticks[slot] = 0;
                world.slot_slide_decel_ticks[slot] = 0;
                world.slot_slide_carry_speed[slot] = 0.f;
                world.slot_brake_cd_ticks[slot] = 0;
                world.slot_steal_cd_ticks[slot] = 0;
                world.slot_pickup_block_ticks[slot] = 0;
                world.slot_one_timer_ticks[slot] = 0;
                world.slot_one_timer_cd_ticks[slot] = 0;
                world.slot_shield_ticks[slot] = 0;
                world.slot_shield_cd_ticks[slot] = 0;
                world.slot_ability_prev[slot] = 0;
                slots.occupied_slots_mask =
                    static_cast<std::uint8_t>(slots.occupied_slots_mask |
                                              static_cast<std::uint8_t>(1U << slot));

                zh::PackedWelcome welcome{};
                welcome.slot_id = static_cast<std::uint8_t>(slot);
                host.send_to_peer(
                    ev.peer,
                    {reinterpret_cast<std::byte const *>(&welcome), sizeof(welcome)},
                    zh::kChannelReliable, ENET_PACKET_FLAG_RELIABLE);
                lobby.lobby_slot_team_b_mask = static_cast<std::uint8_t>(
                    lobby.lobby_slot_team_b_mask &
                    static_cast<std::uint8_t>(~(static_cast<unsigned>(1U << slot))));
                lobby.lobby_slot_goalie_mask = static_cast<std::uint8_t>(
                    lobby.lobby_slot_goalie_mask &
                    static_cast<std::uint8_t>(~(static_cast<unsigned>(1U << slot))));
                broadcast_lobby_teams();
                std::printf("[zh] Client connected — assigned slot %u.\n",
                            static_cast<unsigned>(slot));
                return;
            }

            if (ev.type == ENET_EVENT_TYPE_DISCONNECT && ev.peer != nullptr) {
                std::size_t const s = host_slot_from_peer_data(ev.peer);
                if (s < kRemoteSlots) {
                    slots.slot_active[s] = false;
                    slots.slot_peer[s] = nullptr;
                    host_clear_peer_tag(ev.peer);
                    world.slot_has_waypoint[s] = false;
                    slots.remote_input_schedule[s].clear();
                    if (world.puck_carrier == static_cast<std::int8_t>(s)) {
                        world.puck_carrier = zh::kPuckCarrierFree;
                    }
                    {
                        std::uint8_t const bm = static_cast<std::uint8_t>(1U << s);
                        if ((slots.occupied_slots_mask & bm) != 0) {
                            slots.occupied_slots_mask =
                                static_cast<std::uint8_t>(slots.occupied_slots_mask ^ bm);
                        }
                    }
                    lobby.lobby_slot_team_b_mask = static_cast<std::uint8_t>(
                        lobby.lobby_slot_team_b_mask &
                        static_cast<std::uint8_t>(~(static_cast<unsigned>(1U << s))));
                    lobby.lobby_slot_goalie_mask = static_cast<std::uint8_t>(
                        lobby.lobby_slot_goalie_mask &
                        static_cast<std::uint8_t>(~(static_cast<unsigned>(1U << s))));
                    world.slot_boost_active_ticks[s] = 0;
                    world.slot_boost_cd_ticks[s] = {};
                    world.slot_boost_overspeed_ticks[s] = 0;
                    world.slot_slide_cd_ticks[s] = 0;
                    world.slot_slide_decel_ticks[s] = 0;
                    world.slot_slide_carry_speed[s] = 0.f;
                    world.slot_brake_cd_ticks[s] = 0;
                    world.slot_steal_cd_ticks[s] = 0;
                    world.slot_pickup_block_ticks[s] = 0;
                    world.slot_ability_prev[s] = 0;
                    broadcast_lobby_teams();
                    if (match_running) {
                        on_guest_disconnected_during_match(static_cast<std::uint8_t>(s));
                    }
                }
                return;
            }

            if (ev.type == ENET_EVENT_TYPE_RECEIVE && ev.packet != nullptr) {
                auto *const data = reinterpret_cast<std::uint8_t const *>(ev.packet->data);
                std::size_t const len = ev.packet->dataLength;
                if (len < 1U) {
                    return;
                }
                auto const tag = static_cast<zh::NetMsg>(data[0]);
                if (tag != zh::NetMsg::ClientInput) {
                    return;
                }
                zh::PackedClientInput in{};
                if (!zh::try_copy_net_message(data, len, in)) {
                    return;
                }
                std::size_t const slot = host_slot_from_peer_data(ev.peer);
                if (slot >= kRemoteSlots || !slots.slot_active[slot]) {
                    return;
                }
                if (!match_running) {
                    return;
                }
                if (world.input_delay_ticks_b == 0) {
                    slots.slot_last_seq[slot] = in.seq;
                    slots.slot_move[slot] = in.move_axes;
                    slots.slot_ability[slot] = in.ability_axes;
                    slots.slot_mouse_x[slot] = in.mouse_x;
                    slots.slot_mouse_y[slot] = in.mouse_y;
                    slots.slot_mouse_buttons[slot] = in.mouse_buttons;
                } else {
                    // Schedule for server_tick + 1 + B; drop oldest if queue is full.
                    DelayedRemoteInput pend{};
                    pend.exec_tick =
                        world.server_tick + 1u +
                        static_cast<std::uint32_t>(world.input_delay_ticks_b);
                    pend.seq = in.seq;
                    pend.move_axes = in.move_axes;
                    pend.ability_axes = in.ability_axes;
                    pend.mouse_x = in.mouse_x;
                    pend.mouse_y = in.mouse_y;
                    pend.mouse_buttons = in.mouse_buttons;
                    auto &dq = slots.remote_input_schedule[slot];
                    while (dq.size() >= kMaxDelayedRemoteInputsPerSlot) {
                        dq.pop_front();
                    }
                    dq.push_back(pend);
                }
            }
        },
        timeout_ms);
}

}  // namespace zh::net::host
