// Load/save Options to JSON. Intentionally minimal — no nlohmann dependency for three ints.
#include "zh/app/user_settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace zh::app {

namespace {

// Enough for our hand-written {"udp_port":7777,...} files.
[[nodiscard]] bool parse_uint_field(std::string const &json, char const *key, std::uint32_t &out) {
    std::string const needle = std::string{"\""} + key + "\":";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] < '0' || json[pos] > '9') {
        return false;
    }
    std::uint32_t accum = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        accum = accum * 10U + static_cast<std::uint32_t>(json[pos] - '0');
        ++pos;
    }
    out = accum;
    return true;
}

}  // namespace

std::string default_user_settings_path() {
#if defined(_WIN32)
    if (char const *local = std::getenv("LOCALAPPDATA"); local != nullptr && local[0] != '\0') {
        return std::string{local} + "\\ZHStandalone\\settings.json";
    }
#elif defined(__linux__) || defined(__APPLE__)
    if (char const *xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && xdg[0] != '\0') {
        return std::string{xdg} + "/zh-standalone/settings.json";
    }
    if (char const *home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        return std::string{home} + "/.config/zh-standalone/settings.json";
    }
#endif
    return "settings.json";
}

bool try_load_user_settings(UserSettings &out, std::string const &path) {
    std::ifstream in{path};
    if (!in) {
        return false;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string const json = buf.str();
    if (json.empty()) {
        return false;
    }

    // Start from current out so callers can seed defaults before load.
    UserSettings loaded = out;
    std::uint32_t value = 0;
    if (parse_uint_field(json, "udp_port", value) && value >= 1U && value <= 65535U) {
        loaded.udp_port = static_cast<std::uint16_t>(value);
    }
    if (parse_uint_field(json, "remote_input_delay_ticks", value) && value <= 255U) {
        loaded.remote_input_delay_ticks = static_cast<std::uint8_t>(value);
    }
    if (parse_uint_field(json, "client_interp_delay_ms", value) && value <= 500U) {
        loaded.client_interp_delay_ms = static_cast<std::uint16_t>(value);
    }
    out = loaded;
    return true;
}

bool save_user_settings(UserSettings const &settings, std::string const &path) {
    std::error_code ec;
    std::filesystem::path const file_path{path};
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path(), ec);
    }

    std::ofstream out{path, std::ios::trunc};
    if (!out) {
        return false;
    }
    out << "{\n"
        << "  \"udp_port\": " << static_cast<unsigned>(settings.udp_port) << ",\n"
        << "  \"remote_input_delay_ticks\": "
        << static_cast<unsigned>(settings.remote_input_delay_ticks) << ",\n"
        << "  \"client_interp_delay_ms\": "
        << static_cast<unsigned>(settings.client_interp_delay_ms) << "\n"
        << "}\n";
    return static_cast<bool>(out);
}

}  // namespace zh::app
