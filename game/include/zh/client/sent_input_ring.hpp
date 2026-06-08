#pragma once

// Outbound ClientInput ring: est_exec_tick for replay trim and ack lineage.

#include "zh/protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace zh::client {

inline constexpr std::size_t kSentClientInputRingCap = 256;

// One outbound PackedClientInput plus client-side dequeue estimate.
struct SentClientInputRecord {
    std::uint32_t seq{0};
    // First host execute_tick this packet can plausibly dequeue (~snap_tick + 1 + B + slack).
    std::uint32_t est_exec_tick{0};
    std::uint8_t move_axes{0};
    std::uint8_t ability_axes{0};
    float mouse_x{0.f};
    float mouse_y{0.f};
    std::uint8_t mouse_buttons{0};
};

// Bounded delay queue so a wedged client cannot exhaust host memory (~few seconds @ 60 pps).
class ClientSentInputRing {
public:
    void clear() noexcept;

    // Stamp est_exec_tick from last authoritative server_tick and input_delay_ticks_b.
    void push_sent(PackedClientInput const &in,
                   std::uint32_t snapshot_server_tick,
                   std::uint8_t snapshot_input_delay_ticks_b) noexcept;

    // Keep seq > ack; optional return is ability_axes from max seq still <= ack.
    [[nodiscard]] std::optional<std::uint8_t> trim_to_ack(std::uint32_t ack) noexcept;

    [[nodiscard]] std::size_t count() const noexcept { return count_; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return kSentClientInputRingCap; }

    // i in [0, count): chronological oldest queued send to newest.
    [[nodiscard]] SentClientInputRecord const &at_ordered(std::size_t i_from_oldest) const noexcept;
    [[nodiscard]] SentClientInputRecord &at_ordered(std::size_t i_from_oldest) noexcept;

private:
    [[nodiscard]] std::size_t oldest_slot() const noexcept;

    std::array<SentClientInputRecord, kSentClientInputRingCap> slots_{};
    std::size_t wr_{0};
    std::size_t count_{0};
};

inline std::size_t ClientSentInputRing::oldest_slot() const noexcept {
    return (wr_ + kSentClientInputRingCap - count_) % kSentClientInputRingCap;
}

inline SentClientInputRecord const &ClientSentInputRing::at_ordered(std::size_t const i_from_oldest) const noexcept {
    return slots_[(oldest_slot() + i_from_oldest) % kSentClientInputRingCap];
}

inline SentClientInputRecord &ClientSentInputRing::at_ordered(std::size_t const i_from_oldest) noexcept {
    return slots_[(oldest_slot() + i_from_oldest) % kSentClientInputRingCap];
}

}  // namespace zh::client
