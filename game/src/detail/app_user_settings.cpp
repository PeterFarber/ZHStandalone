// Wire AppContext option fields ↔ zh::app::UserSettings on disk.
#include "detail/app_context.hpp"

#include "zh/app/user_settings.hpp"

#include <cstdio>

namespace zh::detail {

namespace {

std::string const &user_settings_path() {
    static std::string const path = zh::app::default_user_settings_path();
    return path;
}

}  // namespace

void AppContext::load_persisted_user_settings() {
    zh::app::UserSettings settings{};
    settings.udp_port = game_port_;
    settings.remote_input_delay_ticks = stored_remote_delay_ticks_;
    settings.client_interp_delay_ms = stored_client_interp_delay_ms_;

    if (!zh::app::try_load_user_settings(settings, user_settings_path())) {
        return;
    }

    game_port_ = settings.udp_port;
    stored_remote_delay_ticks_ = settings.remote_input_delay_ticks;
    stored_client_interp_delay_ms_ = settings.client_interp_delay_ms;

    // Keep text buffers in sync so Options/Join screens show loaded values.
    options_port_buf_ = std::to_string(static_cast<unsigned>(game_port_));
    options_delay_ticks_buf_ = std::to_string(static_cast<unsigned>(stored_remote_delay_ticks_));
    options_interp_ms_buf_ = std::to_string(static_cast<unsigned>(stored_client_interp_delay_ms_));
    join_port_buf_ = options_port_buf_;
}

bool AppContext::persist_user_settings() const {
    zh::app::UserSettings settings{};
    settings.udp_port = game_port_;
    settings.remote_input_delay_ticks = stored_remote_delay_ticks_;
    settings.client_interp_delay_ms = stored_client_interp_delay_ms_;
    if (zh::app::save_user_settings(settings, user_settings_path())) {
        return true;
    }
    std::fprintf(stderr, "[zh] Could not save settings to %s\n", user_settings_path().c_str());
    return false;
}

}  // namespace zh::detail
