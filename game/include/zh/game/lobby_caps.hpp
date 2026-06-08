#pragma once

// Lobby roster side counts and per-team player/goalie caps.

#include "zh/game/constants.hpp"
#include "zh/protocol.hpp"

namespace zh::game {

struct LobbySideCount {
    unsigned total{};
    unsigned goalies{};
};

[[nodiscard]] LobbySideCount lobby_side_count(bool want_team_b,
                                              std::uint8_t host_team,
                                              std::uint8_t host_goalie_nonzero,
                                              std::uint8_t slot_team_b_mask,
                                              std::uint8_t slot_goalie_mask,
                                              std::uint8_t occ_mask) noexcept;

[[nodiscard]] bool lobby_side_within_caps(LobbySideCount c) noexcept;

[[nodiscard]] bool lobby_both_teams_within_caps(std::uint8_t host_team,
                                                  std::uint8_t host_goalie_nonzero,
                                                  std::uint8_t slot_team_b_mask,
                                                  std::uint8_t slot_goalie_mask,
                                                  std::uint8_t occ_mask) noexcept;

}  // namespace zh::game
