#pragma once

// WAN client local skater prediction, ack follow axes, replay over authority tick.

#include "zh/client/sent_input_ring.hpp"
#include "zh/game/constants.hpp"
#include "zh/protocol.hpp"

#include <cstdint>

namespace zh::client {

class RemoteSkaterPredictor {
public:
    void reset() noexcept;

    // Seed pose/cooldowns from latest snapshot; used before replay_from_hist.
    void authority_resync(std::uint8_t slot,
                          PackedSnapshot const &nw,
                          PackedSnapshot const &prv,
                          float fixed_dt_sec) noexcept;

    void apply_ack_follow_axes(std::uint8_t follow_ability_axes) noexcept;

    // Re-sim sent inputs with exec_tick > authority tick (host delay-B aware).
    void replay_from_hist(ClientSentInputRing const &ring, bool slot_valid, float fixed_dt_sec) noexcept;

    void advance(float frame_dt_sec,
                 float fixed_dt_sec,
                 float mouse_x,
                 float mouse_y,
                 std::uint8_t mouse_buttons,
                 std::uint8_t move_axes,
                 std::uint8_t ability_axes,
                 std::uint8_t my_slot,
                 PackedSnapshot const &snap_authority_latest) noexcept;

    [[nodiscard]] bool seeded() const noexcept { return seeded_; }

    [[nodiscard]] float px() const noexcept { return px_; }
    [[nodiscard]] float py() const noexcept { return py_; }
    [[nodiscard]] float vx() const noexcept { return vx_; }
    [[nodiscard]] float vy() const noexcept { return vy_; }
    [[nodiscard]] float way_x() const noexcept { return way_x_; }
    [[nodiscard]] float way_y() const noexcept { return way_y_; }
    [[nodiscard]] bool has_waypoint() const noexcept { return has_wp_; }
    [[nodiscard]] float face_x() const noexcept { return face_x_; }
    [[nodiscard]] float face_y() const noexcept { return face_y_; }
    [[nodiscard]] std::uint32_t boost_overspeed_ticks() const noexcept { return boost_overspeed_ticks_; }
    [[nodiscard]] std::uint32_t slide_decel_ticks() const noexcept { return slide_decel_ticks_; }
    [[nodiscard]] std::uint32_t one_timer_ticks() const noexcept { return one_timer_ticks_; }
    [[nodiscard]] std::uint32_t one_timer_cd_ticks() const noexcept { return one_timer_cd_ticks_; }
    [[nodiscard]] std::uint32_t shield_ticks() const noexcept { return shield_ticks_; }
    [[nodiscard]] bool is_goalie() const noexcept { return is_goalie_; }
    [[nodiscard]] float one_timer_fraction() const noexcept;
    [[nodiscard]] float shield_fraction() const noexcept;

private:
    void run_hist_step(SentClientInputRecord const &r, std::uint8_t &abl_prev,
                       float fixed_dt_sec) noexcept;
    void consume_boost_frac(float frame_dt_sec, float fixed_dt_sec) noexcept;

    bool seeded_{false};
    float px_{0.f};
    float py_{0.f};
    float vx_{0.f};
    float vy_{0.f};
    float way_x_{0.f};
    float way_y_{0.f};
    bool has_wp_{false};
    float face_x_{1.f};
    float face_y_{0.f};

    std::uint8_t follow_ability_axes_{0};
    std::uint8_t ability_prev_{0};
    std::uint8_t mouse_buttons_prev_{0};

    std::array<std::uint32_t, zh::game::kGoalieBoostCharges> boost_cd_ticks_{};
    std::uint32_t boost_overspeed_ticks_{0};
    std::uint32_t slide_cd_ticks_{0};
    std::uint32_t slide_decel_ticks_{0};
    float slide_carry_speed_{0.f};
    std::uint32_t brake_cd_ticks_{0};
    std::uint32_t one_timer_ticks_{0};
    std::uint32_t one_timer_cd_ticks_{0};
    std::uint32_t shield_ticks_{0};
    std::uint32_t shield_cd_ticks_{0};
    bool is_goalie_{false};
    float boost_tick_frac_accum_{0.f};

    std::uint32_t snap_authority_tick_{0};
};

}  // namespace zh::client
