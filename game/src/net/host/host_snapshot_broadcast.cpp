// Build and send PackedSnapshot on the fixed tick cadence (tick 1, then every N ticks).

#include "zh/net/host/host_snapshot_broadcast.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/snapshot_emit.hpp"

namespace zh::net::host {

bool should_broadcast_match_snapshot(std::uint32_t const server_tick) {
    using zh::game::kSnapshotBroadcastPeriodTicks;
    return (server_tick == 1u) ||
           (((server_tick - 1u) % kSnapshotBroadcastPeriodTicks) == 0u);
}

void broadcast_match_snapshot_if_due(zh::net::ListenHost &host,
                                     HostSnapshotBroadcastContext const &ctx,
                                     std::uint32_t const server_tick) {
    if (!host.valid() || !should_broadcast_match_snapshot(server_tick)) {
        return;
    }

    zh::PackedSnapshot const snap = zh::game::build_host_snapshot(
        ctx.world, ctx.ack_seq_per_slot, ctx.slot_active, ctx.occupied_slots_mask,
        ctx.lobby_host_team, ctx.lobby_slot_team_b_mask, ctx.lobby_host_goalie,
        ctx.lobby_slot_goalie_mask);

    host.broadcast_packet({reinterpret_cast<std::byte const *>(&snap), sizeof(snap)},
                          zh::kChannelGameplay, static_cast<enet_uint32>(0));
}

}  // namespace zh::net::host
