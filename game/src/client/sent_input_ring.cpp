// Ring of outbound ClientInput packets with estimated host execute tick for replay trim.

#include "zh/client/sent_input_ring.hpp"

#include "zh/game/constants.hpp"

namespace zh::client {

void ClientSentInputRing::clear() noexcept {
    wr_ = 0;
    count_ = 0;
    for (auto &s : slots_) {
        s = {};
    }
}

void ClientSentInputRing::push_sent(PackedClientInput const &in,
                                    std::uint32_t snapshot_server_tick,
                                    std::uint8_t snapshot_input_delay_ticks_b) noexcept {
    std::size_t const slot = wr_ % kSentClientInputRingCap;
    slots_[slot].seq = in.seq;
    slots_[slot].est_exec_tick =
        snapshot_server_tick + 1U + static_cast<std::uint32_t>(snapshot_input_delay_ticks_b) +
        zh::game::kClientPredExecTickSlack;
    slots_[slot].move_axes = in.move_axes;
    slots_[slot].ability_axes = in.ability_axes;
    slots_[slot].mouse_x = in.mouse_x;
    slots_[slot].mouse_y = in.mouse_y;
    slots_[slot].mouse_buttons = in.mouse_buttons;
    ++wr_;
    if (count_ < kSentClientInputRingCap) {
        ++count_;
    }
}

// Drop ack'd sends; return ability_axes from highest seq still at or below ack (Z follow lineage).
std::optional<std::uint8_t> ClientSentInputRing::trim_to_ack(std::uint32_t const ack) noexcept {
    constexpr std::size_t cap = kSentClientInputRingCap;
    if (cap == 0 || count_ == 0U) {
        return std::nullopt;
    }

    SentClientInputRecord kept[kSentClientInputRingCap]{};
    std::size_t idx = oldest_slot();

    std::uint32_t best_leq_seq = 0U;
    SentClientInputRecord best_leq{};
    bool have_best_leq = false;
    std::size_t n_kept = 0;
    for (std::size_t step = 0; step < count_; ++step) {
        SentClientInputRecord const &r = slots_[idx];

        if (r.seq <= ack) {
            if (!have_best_leq || r.seq >= best_leq_seq) {
                best_leq = r;
                best_leq_seq = r.seq;
                have_best_leq = true;
            }
        }

        if (r.seq > ack) {
            kept[n_kept++] = r;
        }
        idx = (idx + 1U) % cap;
    }

    for (std::size_t i = 0; i < n_kept; ++i) {
        slots_[i] = kept[i];
    }
    wr_ = n_kept % cap;
    count_ = n_kept;

    if (!have_best_leq) {
        return std::nullopt;
    }
    return best_leq.ability_axes;
}

}  // namespace zh::client
