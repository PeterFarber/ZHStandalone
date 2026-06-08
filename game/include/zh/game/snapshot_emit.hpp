#pragma once

// Build authoritative PackedSnapshot from MatchWorld for host broadcast.

#include "zh/game/match_world.hpp"
#include "zh/protocol.hpp"

#include <array>
#include <cstdint>

namespace zh::game {

// Fills authoritative PackedSnapshot from MatchWorld + per-slot ACK sequence + roster bits.
PackedSnapshot build_host_snapshot(MatchWorld const &world,
                                   std::array<std::uint32_t, kRemoteSlots> const &ack_seq_per_slot,
                                   std::array<bool, kRemoteSlots> const &slot_active,
                                   std::uint8_t occupied_slots_mask,
                                   std::uint8_t lobby_host_team,
                                   std::uint8_t lobby_slot_team_b_mask,
                                   std::uint8_t lobby_host_goalie,
                                   std::uint8_t lobby_slot_goalie_mask);

}  // namespace zh::game
