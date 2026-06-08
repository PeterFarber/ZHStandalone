// Pack MatchWorld + lobby roster bits into authoritative PackedSnapshot for broadcast.

#include "zh/game/snapshot_emit.hpp"

#include "zh/game/skater_physics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace zh::game {

PackedSnapshot build_host_snapshot(MatchWorld const &world,
                                   std::array<std::uint32_t, kRemoteSlots> const &ack_seq_per_slot,
                                   std::array<bool, kRemoteSlots> const &slot_active,
                                   std::uint8_t occupied_slots_mask,
                                   std::uint8_t lobby_host_team,
                                   std::uint8_t lobby_slot_team_b_mask,
                                   std::uint8_t lobby_host_goalie,
                                   std::uint8_t lobby_slot_goalie_mask) {
    PackedSnapshot snap{};
    snap.id = NetMsg::Snapshot;
    snap.server_tick = world.server_tick;
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        snap.ack_seq[i] = ack_seq_per_slot[i];
    }
    snap.puck_x = world.puck_x;
    snap.puck_y = world.puck_y;
    snap.puck_vx = world.puck_vx;
    snap.puck_vy = world.puck_vy;
    snap.puck_carrier = world.puck_carrier;
    snap.host_px = world.host_sk_x;
    snap.host_py = world.host_sk_y;
    snap.host_way_x = world.host_way_x;
    snap.host_way_y = world.host_way_y;
    snap.host_has_waypoint =
        world.host_has_waypoint ? static_cast<std::uint8_t>(1) : static_cast<std::uint8_t>(0);
    snap.slot_occupied_mask = occupied_slots_mask;
    snap.goals_left_side = world.goals_left_side;
    snap.goals_right_side = world.goals_right_side;
    snap.input_delay_ticks_b = world.input_delay_ticks_b;
    snap.match_host_team = static_cast<std::uint8_t>(lobby_host_team & 1U);
    snap.match_slot_team_b_mask = lobby_slot_team_b_mask;
    snap.match_host_goalie = static_cast<std::uint8_t>(lobby_host_goalie & 1U);
    snap.match_slot_goalie_mask = lobby_slot_goalie_mask;
    snap.host_boost_active_ticks = 0;
    // Wire format only carries cooldown mirror; active boost inferred locally during replay.
    snap.host_boost_cd_ticks = boost_ticks_mirror_u8(world.host_boost_cd_ticks[0]);
    snap.host_boost_cd2_ticks = boost_ticks_mirror_u8(world.host_boost_cd_ticks[1]);
    snap.host_one_timer_ticks = boost_ticks_mirror_u8(world.host_one_timer_ticks);
    snap.host_shield_ticks = boost_ticks_mirror_u8(world.host_shield_ticks);
    snap.match_phase = static_cast<std::uint8_t>(world.phase);
    snap.match_period = world.period;
    snap.period_ticks_remaining =
        static_cast<std::uint16_t>(std::min(world.period_ticks_remaining, 65535U));
    snap.faceoff_countdown_ticks =
        static_cast<std::uint8_t>(std::min(world.faceoff_countdown_ticks, 255U));
    for (std::size_t bi = 0; bi < kRemoteSlots; ++bi) {
        if (slot_active[bi]) {
            snap.slot_boost_active_ticks[bi] = 0;
            snap.slot_boost_cd_ticks[bi] =
                boost_ticks_mirror_u8(world.slot_boost_cd_ticks[bi][0]);
            snap.slot_boost_cd2_ticks[bi] =
                boost_ticks_mirror_u8(world.slot_boost_cd_ticks[bi][1]);
            snap.slot_one_timer_ticks[bi] =
                boost_ticks_mirror_u8(world.slot_one_timer_ticks[bi]);
            snap.slot_shield_ticks[bi] = boost_ticks_mirror_u8(world.slot_shield_ticks[bi]);
        } else {
            snap.slot_boost_active_ticks[bi] = 0;
            snap.slot_boost_cd_ticks[bi] = 0;
            snap.slot_boost_cd2_ticks[bi] = 0;
            snap.slot_one_timer_ticks[bi] = 0;
            snap.slot_shield_ticks[bi] = 0;
        }
    }
    snap.skater_waypoint_mask = 0;
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        snap.skater_px[i] = world.slot_sk_x[i];
        snap.skater_py[i] = world.slot_sk_y[i];
        snap.skater_way_x[i] = world.slot_way_x[i];
        snap.skater_way_y[i] = world.slot_way_y[i];
        if (world.slot_has_waypoint[i] && slot_active[i]) {
            snap.skater_waypoint_mask = static_cast<std::uint8_t>(
                snap.skater_waypoint_mask | static_cast<std::uint8_t>(1U << i));
        }
    }

    return snap;
}

}  // namespace zh::game
