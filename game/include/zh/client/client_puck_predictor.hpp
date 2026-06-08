#pragma once

// WAN client predicted puck pose and carrier; reconciles toward latest authoritative snapshot.

#include "zh/client/client_interp_frame.hpp"
#include "zh/game/match_sound_events.hpp"
#include "zh/protocol.hpp"

#include <cstdint>

namespace zh::client {

class ClientPuckPredictor {
public:
    void reset() noexcept;

    void authority_resync(PackedSnapshot const &nw) noexcept;

    // Predict steal/charge/carry using interp peers; reconcile loose puck toward authority.
    [[nodiscard]] zh::game::MatchSoundEvents advance(float frame_dt_sec,
                                                     float fixed_dt_sec,
                                                     ClientInterpFrame const &interp,
                                                     PackedSnapshot const &snap_authority_latest,
                                                     std::uint8_t my_slot,
                                                     float local_sk_px,
                                                     float local_sk_py,
                                                     float local_sk_vx,
                                                     float local_sk_vy,
                                                     float local_sk_face_x,
                                                     float local_sk_face_y,
                                                     float mouse_x,
                                                     float mouse_y,
                                                     std::uint8_t mouse_buttons,
                                                     std::uint32_t local_one_timer_ticks) noexcept;

    [[nodiscard]] bool seeded() const noexcept { return seeded_; }
    [[nodiscard]] float puck_x() const noexcept { return x_; }
    [[nodiscard]] float puck_y() const noexcept { return y_; }
    [[nodiscard]] std::int8_t carrier() const noexcept { return carrier_; }
    [[nodiscard]] float shoot_charge_fraction() const noexcept;
    [[nodiscard]] bool shoot_charging() const noexcept { return shoot_charge_ticks_ > 0.f; }

private:
    static float max_charge_sec_for_ticks() noexcept;

    bool seeded_{false};
    float x_{0.f};
    float y_{0.f};
    float vx_{0.f};
    float vy_{0.f};
    std::int8_t carrier_{kPuckCarrierFree};
    float shoot_charge_ticks_{0.f};
    bool lmb_was_held_{false};
    std::uint32_t steal_cd_ticks_{0};
    std::uint32_t pickup_block_ticks_{0};
    float cd_tick_frac_accum_{0.f};
};

}  // namespace zh::client
