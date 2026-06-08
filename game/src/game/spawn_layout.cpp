// Apply lobby roster (team/goalie picks) to MatchWorld skater positions at match start.

#include "zh/game/spawn_layout.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/skater_physics.hpp"

namespace zh::game {

namespace {

void place_player_spawn(float &px,
                        float &py,
                        bool const team_b,
                        bool const is_goalie,
                        std::size_t const skater_index) noexcept {
    MapPoint const spawn =
        map_runtime().player_spawn_or_default(team_b, is_goalie, skater_index);
    px = spawn.x;
    py = spawn.y;
    clamp_skater_into_rink(px, py, player_draw_radius(is_goalie));
}

}  // namespace

void apply_roster_spawns_to_match(MatchWorld &world,
                                  std::uint8_t const host_team,
                                  std::uint8_t const host_goalie,
                                  std::uint8_t const slot_team_b_mask,
                                  std::uint8_t const slot_goalie_mask,
                                  std::array<bool, kRemoteSlots> const &slot_active) noexcept {
    // Goalies use team.goalie spawn; skaters consume team.skaters[] in join order (max 3 per side).
    world.match_host_goalie = host_goalie & 1U;
    world.match_slot_goalie_mask = slot_goalie_mask;

    bool const host_team_b = host_team != 0;
    std::size_t team_skater_idx[2] = {0U, 0U};  // per-side skater slot index (max 3 each)

    world.host_has_waypoint = false;
    world.host_sk_vx = 0.f;
    world.host_sk_vy = 0.f;
    if ((host_goalie & 1U) != 0U) {
        place_player_spawn(world.host_sk_x, world.host_sk_y, host_team_b, true, 0U);
    } else {
        std::size_t const idx =
            team_skater_idx[host_team_b ? 1U : 0U] < 3U ? team_skater_idx[host_team_b ? 1U : 0U]++ : 2U;
        place_player_spawn(world.host_sk_x, world.host_sk_y, host_team_b, false, idx);
    }

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if (!slot_active[i]) {
            continue;
        }
        bool const team_b =
            (slot_team_b_mask & static_cast<std::uint8_t>(1U << i)) != 0;
        bool const is_goalie =
            (slot_goalie_mask & static_cast<std::uint8_t>(1U << i)) != 0;
        world.slot_has_waypoint[i] = false;
        world.slot_sk_vx[i] = 0.f;
        world.slot_sk_vy[i] = 0.f;
        if (is_goalie) {
            place_player_spawn(world.slot_sk_x[i], world.slot_sk_y[i], team_b, true, 0U);
        } else {
            std::size_t const side = team_b ? 1U : 0U;
            std::size_t const idx =
                team_skater_idx[side] < 3U ? team_skater_idx[side]++ : 2U;
            place_player_spawn(world.slot_sk_x[i], world.slot_sk_y[i], team_b, false, idx);
        }
    }
}

}  // namespace zh::game
