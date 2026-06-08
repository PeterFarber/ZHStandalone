// Strict digit parsers for lobby form fields (port, delay ticks, interp ms).

#include "zh/ui/form_parse.hpp"

#include "zh/game/constants.hpp"

namespace zh::ui {

bool parse_udp_port_string(std::string const &text, std::uint16_t &out_port) {
    std::uint32_t accum = 0;
    bool any = false;
    for (char const ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        any = true;
        accum = accum * 10U + static_cast<std::uint32_t>(ch - '0');
        if (accum > 65535U) {
            return false;
        }
    }
    if (!any) {
        return false;
    }
    if (accum == 0U) {
        return false;
    }
    out_port = static_cast<std::uint16_t>(accum);
    return true;
}

bool parse_delay_ticks_string(std::string const &text, std::uint8_t &out_ticks) noexcept {
    std::uint32_t accum = 0;
    bool any_digit = false;
    for (char const ch : text) {
        if (ch == ' ') {
            continue;
        }
        if (ch < '0' || ch > '9') {
            return false;
        }
        any_digit = true;
        accum = accum * 10U + static_cast<std::uint32_t>(ch - '0');
        if (accum > 255U) {
            accum = static_cast<std::uint32_t>(zh::game::kMaxInputDelayTicks);
            break;
        }
    }
    if (!any_digit) {
        out_ticks = 0;
        return true;
    }
    if (accum > static_cast<std::uint32_t>(zh::game::kMaxInputDelayTicks)) {
        accum = static_cast<std::uint32_t>(zh::game::kMaxInputDelayTicks);
    }
    out_ticks = static_cast<std::uint8_t>(accum);
    return true;
}

bool parse_interp_delay_ms_string(std::string const &text,
                                  std::uint16_t max_ms,
                                  std::uint16_t &out_ms) noexcept {
    std::uint32_t accum = 0;
    bool any_digit = false;
    for (char const ch : text) {
        if (ch == ' ') {
            continue;
        }
        if (ch < '0' || ch > '9') {
            return false;
        }
        any_digit = true;
        accum = accum * 10U + static_cast<std::uint32_t>(ch - '0');
        if (accum > static_cast<std::uint32_t>(max_ms)) {
            accum = static_cast<std::uint32_t>(max_ms);
        }
    }
    if (!any_digit) {
        return false;
    }
    out_ms = static_cast<std::uint16_t>(accum);
    return true;
}

}  // namespace zh::ui
