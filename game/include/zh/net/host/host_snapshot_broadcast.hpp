#pragma once

// Throttled PackedSnapshot broadcast from host MatchWorld state (see kSnapshotBroadcastPeriodTicks).

#include "zh/game/match_world.hpp"
#include "zh/network.hpp"
#include "zh/protocol.hpp"

#include <array>
#include <cstdint>

namespace zh::net::host {

struct HostSnapshotBroadcastContext {
    zh::game::MatchWorld const &world;
    std::array<std::uint32_t, kRemoteSlots> const &ack_seq_per_slot;
    std::array<bool, kRemoteSlots> const &slot_active;
    std::uint8_t occupied_slots_mask;
    std::uint8_t lobby_host_team;
    std::uint8_t lobby_slot_team_b_mask;
    std::uint8_t lobby_host_goalie;
    std::uint8_t lobby_slot_goalie_mask;
};

[[nodiscard]] bool should_broadcast_match_snapshot(std::uint32_t server_tick);

void broadcast_match_snapshot_if_due(zh::net::ListenHost &host,
                                     HostSnapshotBroadcastContext const &ctx,
                                     std::uint32_t server_tick);

}  // namespace zh::net::host
