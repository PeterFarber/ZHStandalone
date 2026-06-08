#pragma once

// UDP message layouts. Packed structs — changing size breaks the wire protocol.
// Channel 0 = unreliable (inputs, snapshots). Channel 1 = reliable (welcome, lobby).

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace zh {

constexpr std::size_t kRemoteSlots = 8;

constexpr std::size_t kChannelGameplay = 0;
constexpr std::size_t kChannelReliable = 1;

constexpr std::int8_t kPuckCarrierFree = -1;
constexpr std::int8_t kPuckCarrierHost = -2;  // host skater carries puck (not a slot index)

#pragma pack(push, 1)

constexpr std::uint8_t kClientMouseRmbClick = static_cast<std::uint8_t>(1U << 0U);
constexpr std::uint8_t kClientMouseLmbClick = static_cast<std::uint8_t>(1U << 1U);
constexpr std::uint8_t kClientMouseLmbHeld = kClientMouseLmbClick;

constexpr std::uint8_t kClientAbilityZ = static_cast<std::uint8_t>(1U << 0U);
constexpr std::uint8_t kClientAbilityX = static_cast<std::uint8_t>(1U << 1U);
constexpr std::uint8_t kClientAbilitySBrake = static_cast<std::uint8_t>(1U << 2U);
constexpr std::uint8_t kClientAbilityCOneTimer = static_cast<std::uint8_t>(1U << 3U);

enum class NetMsg : std::uint8_t {
    Welcome = 1,
    ClientInput = 2,
    Snapshot = 3,
    LobbyTeams = 4,
};

struct PackedWelcome {
    NetMsg id{NetMsg::Welcome};
    std::uint8_t slot_id{255};
};

struct PackedLobbyTeams {
    NetMsg id{NetMsg::LobbyTeams};
    std::uint8_t host_team{0};           // 0 = team A, 1 = team B
    std::uint8_t slot_team_mask{0};      // bit i: slot i on team B
    std::uint8_t slot_occupied_mask{0};
    std::uint8_t host_goalie{0};
    std::uint8_t slot_goalie_mask{0};
};

struct PackedClientInput {
    NetMsg id{NetMsg::ClientInput};
    std::uint32_t seq{0};
    std::uint8_t move_axes{0};     // arrow keys, bits 0-3
    std::uint8_t ability_axes{0}; // Z/X/S/C held
    float mouse_x{0.f};
    float mouse_y{0.f};            // world space on the rink
    std::uint8_t mouse_buttons{0}; // RMB click, LMB hold
};

struct PackedSnapshot {
    NetMsg id{NetMsg::Snapshot};
    std::uint32_t server_tick{0};
    std::uint32_t ack_seq[kRemoteSlots]{};
    float puck_x{0.f};
    float puck_y{0.f};
    float puck_vx{0.f};
    float puck_vy{0.f};
    std::int8_t puck_carrier{kPuckCarrierFree};
    float host_px{0.f};
    float host_py{0.f};
    float host_way_x{0.f};
    float host_way_y{0.f};
    std::uint8_t host_has_waypoint{0};
    float skater_px[kRemoteSlots]{};
    float skater_py[kRemoteSlots]{};
    float skater_way_x[kRemoteSlots]{};
    float skater_way_y[kRemoteSlots]{};
    std::uint8_t skater_waypoint_mask{0};
    std::uint8_t slot_occupied_mask{0};
    std::uint8_t goals_left_side{0};
    std::uint8_t goals_right_side{0};
    std::uint8_t input_delay_ticks_b{0};  // host "B" delay, replicated so clients can predict
    std::uint8_t match_host_team{0};
    std::uint8_t match_slot_team_b_mask{0};
    std::uint8_t match_host_goalie{0};
    std::uint8_t match_slot_goalie_mask{0};
    std::uint8_t host_boost_active_ticks{0};
    std::uint8_t host_boost_cd_ticks{0};
    std::uint8_t host_boost_cd2_ticks{0};
    std::uint8_t slot_boost_active_ticks[kRemoteSlots]{};
    std::uint8_t slot_boost_cd_ticks[kRemoteSlots]{};
    std::uint8_t slot_boost_cd2_ticks[kRemoteSlots]{};
    std::uint8_t host_one_timer_ticks{0};
    std::uint8_t slot_one_timer_ticks[kRemoteSlots]{};
    std::uint8_t host_shield_ticks{0};
    std::uint8_t slot_shield_ticks[kRemoteSlots]{};
    std::uint8_t match_phase{0};       // MatchPhase as uint8
    std::uint8_t match_period{1};
    std::uint16_t period_ticks_remaining{0};
    std::uint8_t faceoff_countdown_ticks{0};
};

#pragma pack(pop)

static_assert(sizeof(PackedWelcome) == 2);
static_assert(sizeof(PackedLobbyTeams) == 6);
static_assert(sizeof(PackedClientInput) == 16);
static_assert(sizeof(PackedSnapshot) == 258);  // update clients if this changes

template <class T>
[[nodiscard]] bool try_copy_net_message(std::uint8_t const *data,
                                        std::size_t length,
                                        T &out) {
    if (data == nullptr || length < sizeof(T)) return false;
    std::memcpy(&out, data, sizeof(T));
    return true;
}

}  // namespace zh
