#pragma once

// Place host and remote skaters from lobby roster onto map spawns at match start.

#include "zh/game/match_world.hpp"
#include "zh/protocol.hpp"

#include <array>
#include <cstdint>

namespace zh::game {

void apply_roster_spawns_to_match(MatchWorld &world,
                                  std::uint8_t host_team,
                                  std::uint8_t host_goalie,
                                  std::uint8_t slot_team_b_mask,
                                  std::uint8_t slot_goalie_mask,
                                  std::array<bool, kRemoteSlots> const &slot_active) noexcept;

}  // namespace zh::game
