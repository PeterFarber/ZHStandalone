#pragma once

// Authoritative match tuning: simulation, client prediction, lobby caps, and HUD.
// Durations in "ticks" use the fixed sim step (60 Hz; see zh::App::kFixedDt).
// Distances and speeds are in world units unless noted.

#include "zh/game/rink_layout.hpp"

#include <cstddef>
#include <cstdint>

namespace zh::game {

// =============================================================================
// Match clock & faceoffs
// =============================================================================

inline constexpr int kMatchPeriodCount = 3;
// Wall-clock length of one regulation period at 60 Hz (180 s).
inline constexpr std::uint32_t kMatchPeriodDurationTicks = 180U * 60U;
// Post-goal / period-start faceoff: puck stays dead for 3 s (180 ticks).
inline constexpr std::uint32_t kMatchFaceoffCountdownTicks = 3U * 60U;

[[nodiscard]] inline int faceoff_countdown_secs_from_ticks(std::uint32_t ticks) noexcept {
    if (ticks == 0U) {
        return 0;
    }
    return static_cast<int>((ticks + 59U) / 60U);
}

// =============================================================================
// Lobby roster limits
// =============================================================================

inline constexpr unsigned kLobbyMaxPlayersPerSide = 4U;
inline constexpr unsigned kLobbyMaxGoaliesPerSide = 1U;

// =============================================================================
// Network input & snapshot cadence
// =============================================================================

// Max frames of input delay the client may apply (48 ticks = 0.8 s @ 60 Hz).
inline constexpr std::uint8_t kMaxInputDelayTicks = static_cast<std::uint8_t>(48U);
// Host sends unreliable snapshots every N sim ticks.
inline constexpr std::uint32_t kSnapshotBroadcastPeriodTicks = 3U;
// Extra slack beyond recv_exec ~= snap_tick + 1 + B when throttling client replay.
inline constexpr std::uint32_t kClientPredExecTickSlack = kSnapshotBroadcastPeriodTicks + 3U;

// =============================================================================
// Presentation — collision & sprite sizes
// =============================================================================

inline constexpr float kPuckDrawRadius = 9.f;
inline constexpr float kSkaterDrawRadius = 12.f;
inline constexpr float kGoalieDrawRadius = 16.f;
inline constexpr float kSkaterSpriteDrawPx = 32.f;
inline constexpr float kGoalieSpriteDrawPx = 42.f;
// One background repeat tile: texture scales into this many world units (smaller = denser repeat).
inline constexpr float kBackgroundTileTexels = 48.f;

// =============================================================================
// Skater movement
// =============================================================================

inline constexpr float kSkaterAccel = 250.f;
inline constexpr float kSkaterMaxSpeed = 180.f;
inline constexpr float kSkaterFrictionCoast = 0.993f;  // per-tick multiplier while coasting
inline constexpr float kSkaterStopSpeed = 14.f;          // below this, treat as stopped
// End-board / goal-frame bounce (same restitution as loose puck on boards).
inline constexpr float kSkaterWallBounceFactor = 0.98f;
// Below this speed, blade direction uses stored facing instead of velocity.
inline constexpr float kSkaterMoveFacingSpeed = 42.f;

// =============================================================================
// Goalie movement (larger hitbox, slower than skaters)
// =============================================================================

inline constexpr float kGoalieAccel = 195.f;
inline constexpr float kGoalieMaxSpeed = 138.f;
// Goalies get two independent boost charges (each with its own cooldown).
inline constexpr unsigned kGoalieBoostCharges = 2U;

// =============================================================================
// Role helpers — skater vs goalie at runtime
// =============================================================================

[[nodiscard]] inline constexpr float player_draw_radius(bool is_goalie) noexcept {
    return is_goalie ? kGoalieDrawRadius : kSkaterDrawRadius;
}

[[nodiscard]] inline constexpr float player_max_speed(bool is_goalie) noexcept {
    return is_goalie ? kGoalieMaxSpeed : kSkaterMaxSpeed;
}

[[nodiscard]] inline constexpr float player_accel(bool is_goalie) noexcept {
    return is_goalie ? kGoalieAccel : kSkaterAccel;
}

[[nodiscard]] inline constexpr unsigned player_boost_charge_count(bool is_goalie) noexcept {
    return is_goalie ? kGoalieBoostCharges : 1U;
}

[[nodiscard]] inline constexpr float player_sprite_draw_px(bool is_goalie) noexcept {
    return is_goalie ? kGoalieSpriteDrawPx : kSkaterSpriteDrawPx;
}

// =============================================================================
// Boost (double-tap Z)
// =============================================================================

inline constexpr float kSkaterBoostImpulse = 400.f;
inline constexpr std::uint32_t kSkaterBoostCooldownTicks = 300U;  // 5 s @ 60 Hz; per charge for goalies
// If speed is below this, boost uses stored facing instead of velocity.
inline constexpr float kSkaterBoostMinDirSpeed = 12.f;
// Brief window where max speed may exceed normal cap after the Z impulse.
inline constexpr float kSkaterBoostOverspeed = 90.f;
inline constexpr std::uint32_t kSkaterBoostOverspeedDurationTicks = 20U;

// =============================================================================
// Brake tap (S)
// =============================================================================

inline constexpr float kSkaterBrakeDrag = 0.58f;
// Same cadence as slide: 0.5 s @ 60 Hz.
inline constexpr std::uint32_t kSkaterBrakeCooldownTicks = 30U;

// =============================================================================
// Slide stop (X — skaters only; goalies use shield on X)
// =============================================================================

// Banks current speed for the next RMB waypoint leg; gradual decel over decel window.
inline constexpr std::uint32_t kSkaterSlideCooldownTicks = 30U;   // 0.5 s @ 60 Hz
inline constexpr std::uint32_t kSkaterSlideDecelTicks = 20U;
inline constexpr float kSkaterSlideDecelPerTick = 0.972f;
// Next RMB leg launch speed = max_speed * this (turn cap), regardless of entry speed.
inline constexpr float kSkaterSlideNextLegSpeedCap = 0.25f;

// =============================================================================
// Goalie shield (X)
// =============================================================================

// Large circle around goalie; loose pucks reflect off the edge.
inline constexpr float kGoalieShieldRadius = kGoalieDrawRadius + 26.f;
inline constexpr std::uint32_t kGoalieShieldDurationTicks = 72U;    // 1.2 s @ 60 Hz
inline constexpr std::uint32_t kGoalieShieldCooldownTicks = 210U;   // 3.5 s @ 60 Hz
inline constexpr float kGoalieShieldBounce = 1.02f;

// =============================================================================
// One-timer redirect (C)
// =============================================================================

// Window to redirect a loose puck toward the cursor at its current speed.
inline constexpr std::uint32_t kSkaterOneTimerDurationTicks = 90U;    // 1.5 s @ 60 Hz
inline constexpr std::uint32_t kSkaterOneTimerCooldownTicks = 180U;  // 3 s @ 60 Hz

// =============================================================================
// Shooting (LMB hold / release)
// =============================================================================

inline constexpr std::uint32_t kShootCooldownTicks = 18U;
// Hold up to this many ticks; release fires a charged slap.
inline constexpr std::uint32_t kShootChargeMaxTicks = 45U;
inline constexpr float kShootImpulseMin = 180.f;
inline constexpr float kShootImpulseMax = 920.f;
// Cursor distance from skater at which aim scaling reaches full power.
inline constexpr float kShootAimDistForMax = 420.f;
inline constexpr float kShootAimDistMinFactor = 0.25f;

// =============================================================================
// Puck carry, steal, and slap pickup
// =============================================================================

inline constexpr float kMatchSlapImpulse = 420.f;
inline constexpr float kMatchStealNudge = 105.f;
inline constexpr float kMatchStealDamp = 0.82f;
inline constexpr std::uint32_t kPuckStealCooldownTicks = 6U;       // 0.1 s @ 60 Hz
// After a successful steal, delay before LMB can charge or release a shot.
inline constexpr std::uint32_t kShootAfterStealBlockTicks = 6U;   // 0.1 s @ 60 Hz
// Shooter cannot overlap-pickup their own loose puck immediately after release.
inline constexpr std::uint32_t kPuckSelfPickupBlockTicks = 6U;     // 0.1 s @ 60 Hz

// =============================================================================
// Loose puck physics
// =============================================================================

inline constexpr float kPuckFloorFriction = 0.993f;  // per-tick multiplier on ice
inline constexpr float kPuckStopSpeed = 14.f;

// =============================================================================
// Stick blade & steal hitbox
// =============================================================================

inline constexpr float kBladeAhead = 26.f;
// Circle ahead of skater along blade axis — used for steal vs enemy body.
inline constexpr float kStickHitboxAhead = 28.f;
inline constexpr float kStickHitboxRadius = 14.f;

// =============================================================================
// Waypoint skating (RMB legs)
// =============================================================================

inline constexpr float kWaypointArrivalDistSq = 400.f;  // 20 world units squared
inline constexpr float kWaypointArrivalSpeed = 52.f;

// =============================================================================
// Client prediction — remote skaters
// =============================================================================

inline constexpr float kClientPredictAuthSnapPullPerSec = 4.f;
inline constexpr std::size_t kClientPredReplayPhysicsBudgetPerSnap = 12U;
inline constexpr std::size_t kClientPredReplayMaxSteps = 80U;

// =============================================================================
// Client prediction — puck & snapshot reconciliation
// =============================================================================

inline constexpr float kClientPredictPuckPullPerSec = 7.f;
inline constexpr float kClientPredictPullBdampPerTick = 0.38f;

}  // namespace zh::game
