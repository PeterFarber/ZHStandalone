#pragma once

// Authoritative host-side match sim: puck, skaters, period clock. No gfx or ENet here.

#include "zh/game/constants.hpp"
#include "zh/game/match_sound_events.hpp"
#include "zh/protocol.hpp"

#include <array>
#include <cstdint>

namespace zh::game {

struct HostLocalTickInput {
    std::uint8_t move_axes{0};
    std::uint8_t mouse_buttons{0};
    std::uint8_t ability_axes{0};
    float mouse_x{0.f};
    float mouse_y{0.f};
};

// Per-tick inputs gathered from AppContext slot tables before one host step.
struct HostMatchTickInputs {
    HostLocalTickInput local{};
    std::array<std::uint8_t, kRemoteSlots> slot_mouse_buttons{};
    std::array<std::uint8_t, kRemoteSlots> slot_ability{};
    std::array<float, kRemoteSlots> slot_mouse_x{};
    std::array<float, kRemoteSlots> slot_mouse_y{};
};

enum class MatchPhase : std::uint8_t {
    FaceoffCountdown = 0,
    Playing = 1,
    MatchEnded = 2,
};

struct HostMatchTickResult {
    bool scored_goal{false};
    MatchSoundEvents sounds{};
};

// Puck + skater state and match clock. Peer/slot tables live on AppContext.
struct MatchWorld {
    std::uint32_t server_tick{0};

    float puck_x{640.f};
    float puck_y{360.f};
    float puck_vx{0.f};
    float puck_vy{0.f};
    std::int8_t puck_carrier{kPuckCarrierFree};

    float host_sk_x{0.f};
    float host_sk_y{0.f};
    float host_way_x{0.f};
    float host_way_y{0.f};
    bool host_has_waypoint{false};
    float host_sk_vx{0.f};
    float host_sk_vy{0.f};
    float host_facing_x{1.f};
    float host_facing_y{0.f};

    std::array<float, kRemoteSlots> slot_sk_x{};
    std::array<float, kRemoteSlots> slot_sk_y{};
    std::array<float, kRemoteSlots> slot_way_x{};
    std::array<float, kRemoteSlots> slot_way_y{};
    std::array<bool, kRemoteSlots> slot_has_waypoint{};

    std::array<float, kRemoteSlots> slot_sk_vx{};
    std::array<float, kRemoteSlots> slot_sk_vy{};
    std::array<float, kRemoteSlots> slot_facing_x{};
    std::array<float, kRemoteSlots> slot_facing_y{};

    std::uint8_t goals_left_side{0};
    std::uint8_t goals_right_side{0};

    MatchPhase phase{MatchPhase::FaceoffCountdown};
    std::uint8_t period{1};
    std::uint32_t period_ticks_remaining{kMatchPeriodDurationTicks};
    std::uint32_t faceoff_countdown_ticks{0};

    // WAN input delay B (ticks); mirrored in PackedSnapshot for clients.
    std::uint8_t input_delay_ticks_b{0};

    std::uint32_t host_boost_active_ticks{0};
    std::array<std::uint32_t, kGoalieBoostCharges> host_boost_cd_ticks{};
    std::uint32_t host_boost_overspeed_ticks{0};
    std::uint32_t host_slide_cd_ticks{0};
    std::uint32_t host_slide_decel_ticks{0};
    float host_slide_carry_speed{0.f};
    std::uint32_t host_brake_cd_ticks{0};
    std::uint32_t host_steal_cd_ticks{0};
    std::uint32_t host_pickup_block_ticks{0};
    std::uint32_t host_one_timer_ticks{0};
    std::uint32_t host_one_timer_cd_ticks{0};
    std::uint32_t host_shield_ticks{0};
    std::uint32_t host_shield_cd_ticks{0};
    std::array<std::uint32_t, kRemoteSlots> slot_boost_active_ticks{};
    std::array<std::array<std::uint32_t, kGoalieBoostCharges>, kRemoteSlots> slot_boost_cd_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_boost_overspeed_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_slide_cd_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_slide_decel_ticks{};
    std::array<float, kRemoteSlots> slot_slide_carry_speed{};
    std::array<std::uint32_t, kRemoteSlots> slot_brake_cd_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_steal_cd_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_pickup_block_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_one_timer_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_one_timer_cd_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_shield_ticks{};
    std::array<std::uint32_t, kRemoteSlots> slot_shield_cd_ticks{};

    std::uint8_t match_host_goalie{0};
    std::uint8_t match_slot_goalie_mask{0};

    std::uint8_t host_ability_prev{0};
    std::array<std::uint8_t, kRemoteSlots> slot_ability_prev{};

    std::uint32_t host_shoot_cooldown{0};
    std::array<std::uint32_t, kRemoteSlots> slot_shoot_cooldown{};

    std::uint8_t host_mouse_prev{0};
    std::uint32_t host_shoot_charge_ticks{0};
    std::array<std::uint32_t, kRemoteSlots> slot_shoot_charge_ticks{};

    void reset_session(float screen_w, float screen_h);

    // Host pressed Start; roster spawns must already be applied.
    void begin_match() noexcept;

    void tick_match_clock(std::array<bool, kRemoteSlots> const &slot_active,
                          float screen_w,
                          float screen_h) noexcept;

    void apply_face_off_anchors(float screen_w,
                                float screen_h,
                                std::array<bool, kRemoteSlots> const &slot_active) noexcept;

    void decrement_match_cooldowns() noexcept;

    void tick_end_boost_and_abilities(std::array<bool, kRemoteSlots> const &slot_active,
                                      std::uint8_t host_ability_now,
                                      std::array<std::uint8_t, kRemoteSlots> const &slot_ability_now);

    [[nodiscard]] float host_shoot_charge_fraction() const noexcept;
    [[nodiscard]] float host_one_timer_fraction() const noexcept;
    [[nodiscard]] float host_shield_fraction() const noexcept;

    // One fixed host tick: movement, puck, shots, goals. Updates remote_mouse_seen for edge detect.
    HostMatchTickResult tick_host_match(HostMatchTickInputs const &in,
                                        float dt,
                                        std::array<bool, kRemoteSlots> const &slot_active,
                                        std::array<std::uint8_t, kRemoteSlots> &remote_mouse_seen,
                                        float screen_w,
                                        float screen_h);
};

}  // namespace zh::game
