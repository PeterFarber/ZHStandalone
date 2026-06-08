#pragma once

// Strict digit parsers for lobby form fields (port, delay ticks, interp ms).

#include <cstdint>
#include <string>

namespace zh::ui {

[[nodiscard]] bool parse_udp_port_string(std::string const &text, std::uint16_t &out_port);

// Parse 0…kMaxInputDelayTicks WAN delay (B ticks). Empty field → 0.
[[nodiscard]] bool parse_delay_ticks_string(std::string const &text, std::uint8_t &out_ticks) noexcept;

// Parse 0…max_ms client-only interpolation lag (at least one digit required).
[[nodiscard]] bool parse_interp_delay_ms_string(std::string const &text,
                                                std::uint16_t max_ms,
                                                std::uint16_t &out_ms) noexcept;

}  // namespace zh::ui
