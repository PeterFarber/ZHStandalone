#pragma once

#include <cstdint>
#include <string>

namespace zh::app {

// Persisted Options screen values. Written as a small JSON file on disk.
struct UserSettings {
    std::uint16_t udp_port{7777};
    std::uint8_t remote_input_delay_ticks{0};   // host-only "B" ticks for remote inputs
    std::uint16_t client_interp_delay_ms{88};   // WAN client replay lag (not networked)
};

// OS-specific path; falls back to ./settings.json if env vars are missing.
[[nodiscard]] std::string default_user_settings_path();

// Missing file or bad JSON → false and out unchanged. Known fields merge onto out.
[[nodiscard]] bool try_load_user_settings(UserSettings &out, std::string const &path);

[[nodiscard]] bool save_user_settings(UserSettings const &settings, std::string const &path);

}  // namespace zh::app
