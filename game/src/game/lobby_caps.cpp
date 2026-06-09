// Lobby roster validation: per-side player and goalie caps before match start.

#include "zh/game/lobby_caps.hpp"

#include "zh/game/constants.hpp"
#include "zh/protocol.hpp"

#include <cstddef>

namespace zh::game {

// Count host + occupied remote slots on one team side (west = !team_b, east = team_b).
LobbySideCount lobby_side_count(bool want_team_b,
                                std::uint8_t host_team,
                                std::uint8_t host_goalie_nonzero,
                                std::uint8_t slot_team_b_mask,
                                std::uint8_t slot_goalie_mask,
                                std::uint8_t occ_mask) noexcept {
    LobbySideCount c{};
    bool const host_on_b = host_team != 0;
    if (host_on_b == want_team_b) {
        ++c.total;
        if ((host_goalie_nonzero & 1U) != 0U) {
            ++c.goalies;
        }
    }
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if ((occ_mask & static_cast<std::uint8_t>(1U << i)) == 0) {
            continue;
        }
        bool const slot_side_b =
            (slot_team_b_mask & static_cast<std::uint8_t>(1U << i)) != 0;
        if (slot_side_b != want_team_b) {
            continue;
        }
        ++c.total;
        if ((slot_goalie_mask & static_cast<std::uint8_t>(1U << i)) != 0) {
            ++c.goalies;
        }
    }
    return c;
}

bool lobby_side_within_caps(LobbySideCount c) noexcept {
    return c.total <= kLobbyMaxPlayersPerSide && c.goalies <= kLobbyMaxGoaliesPerSide &&
           c.goalies <= c.total;
}

bool lobby_both_teams_within_caps(std::uint8_t host_team,
                                  std::uint8_t host_goalie_nonzero,
                                  std::uint8_t slot_team_b_mask,
                                  std::uint8_t slot_goalie_mask,
                                  std::uint8_t occ_mask) noexcept {
    LobbySideCount const west =
        lobby_side_count(false, host_team, host_goalie_nonzero, slot_team_b_mask,
                         slot_goalie_mask, occ_mask);
    LobbySideCount const east =
        lobby_side_count(true, host_team, host_goalie_nonzero, slot_team_b_mask,
                         slot_goalie_mask, occ_mask);
    return lobby_side_within_caps(west) && lobby_side_within_caps(east);
}

}  // namespace zh::game
