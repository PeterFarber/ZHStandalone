// FIFO ring of received snapshots with wall-clock time for render interpolation.

#include "zh/client/snap_ring_buffer.hpp"

#include <algorithm>

namespace zh::client {

void ClientSnapRingBuffer::clear() noexcept {
    wr_ = 0;
    count_ = 0;
    for (auto &s : slots_) {
        s = {};
    }
}

void ClientSnapRingBuffer::push(PackedSnapshot const &snap, double const time_sec) noexcept {
    std::size_t const slot = wr_ % kSnapHistoryCap;
    slots_[slot].snap = snap;
    slots_[slot].time_sec = time_sec;
    ++wr_;
    if (count_ < kSnapHistoryCap) {
        ++count_;
    }
}

// Pick bracketing snapshots at (wall_time - replay_lag); mix_t lerps between them.
void ClientSnapRingBuffer::sample_interp_frame(double const wall_time_sec,
                                               std::uint16_t replay_lag_ms,
                                               PackedSnapshot const &seed_newer,
                                               bool const wan_client_active,
                                               ClientInterpFrame &out) const {
    out.older_snap = seed_newer;
    out.newer_snap = seed_newer;
    out.mix_t = 1.f;
    if (!wan_client_active || count_ < 2U) {
        return;
    }

    double const replay_lag_s = static_cast<double>(replay_lag_ms) * 1e-3;
    double const sample_time = wall_time_sec - replay_lag_s;

    std::size_t const n = std::min(count_, kSnapHistoryCap);
    auto const ring_idx_back = [&](std::size_t const step_from_newest) -> std::size_t {
        return (wr_ + kSnapHistoryCap - 1u - step_from_newest) % kSnapHistoryCap;
    };

    std::size_t const newest_i = ring_idx_back(0U);
    double cand_new_t = slots_[newest_i].time_sec;

    if (sample_time >= cand_new_t) {
        PackedSnapshot const s = slots_[newest_i].snap;
        out.older_snap = s;
        out.newer_snap = s;
        out.mix_t = 1.f;
        return;
    }

    PackedSnapshot cand_new_snap = slots_[newest_i].snap;

    for (std::size_t step = 1; step < n; ++step) {
        std::size_t const idx = ring_idx_back(step);
        double const ot = slots_[idx].time_sec;
        PackedSnapshot const older_cand = slots_[idx].snap;

        if (ot <= sample_time) {
            double const span = cand_new_t - ot;
            float const frac =
                span > 1e-5 ? static_cast<float>((sample_time - ot) / span) : 1.f;
            out.older_snap = older_cand;
            out.newer_snap = cand_new_snap;
            out.mix_t = std::clamp(frac, 0.f, 1.f);
            return;
        }
        cand_new_snap = older_cand;
        cand_new_t = ot;
    }

    std::size_t const oldest_i = ring_idx_back(n - 1U);
    PackedSnapshot const s = slots_[oldest_i].snap;
    out.older_snap = s;
    out.newer_snap = s;
    out.mix_t = 0.f;
}

}  // namespace zh::client
