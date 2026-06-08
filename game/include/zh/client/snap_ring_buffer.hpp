#pragma once

// Received PackedSnapshot history with receive timestamps for client interpolation.

#include "zh/client/client_interp_frame.hpp"
#include "zh/protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace zh::client {

inline constexpr std::size_t kSnapHistoryCap = 64;

// FIFO ring of received PackedSnapshot with host receive/wall timestamps (client-only).
class ClientSnapRingBuffer {
public:
    void clear() noexcept;
    void push(PackedSnapshot const &snap, double time_sec) noexcept;

    [[nodiscard]] std::size_t count() const noexcept { return count_; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return kSnapHistoryCap; }

    // Build ClientInterpFrame for render (WAN replay lag in ms). Fewer than two samples duplicates seed.
    void sample_interp_frame(double wall_time_sec,
                             std::uint16_t replay_lag_ms,
                             PackedSnapshot const &seed_newer,
                             bool wan_client_active,
                             ClientInterpFrame &out) const;

private:
    struct Slot {
        PackedSnapshot snap{};
        double time_sec{0.};
    };
    std::array<Slot, kSnapHistoryCap> slots_{};
    std::size_t wr_{0};
    std::size_t count_{0};
};

}  // namespace zh::client
