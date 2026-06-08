#pragma once

// Everything the game needs at runtime that isn't worth exposing in zh::App.
// One instance per process — host and client modes share this struct.

#include "zh/win32_fix_net_order.h"

#include "zh/client/wan_client_session.hpp"

#include <raylib.h>

#include "zh/game/match_world.hpp"
#include "zh/ui/rink_sprites.hpp"
#include "zh/ui/skater_fx.hpp"
#include "zh/ui/game_sounds.hpp"

#include "zh/net/host/host_gameplay_router.hpp"
#include "zh/network.hpp"
#include "zh/protocol.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

namespace zh::detail {

inline constexpr std::uint16_t kDefaultClientInterpDelayMs = 88U;
inline constexpr std::uint16_t kMaxClientInterpDelayMs = 500U;  // Options UI cap

enum class Screen : std::uint8_t {
    Home,
    JoinForm,
    HostLobby,
    ClientLobby,
    Options,
    Playing,
};

enum class JoinEditField : std::uint8_t {
    Ip,
    Port,
};

enum class OptionsEditField : std::uint8_t {
    UdpPort,
    RemoteInputDelayTicks,
    ClientInterpLagMs,
};

// One frame of local input sent to sim (host) or packed into ClientInput (WAN client).
struct LocalPlayingInputSample {
    std::uint8_t move_axes{0};
    std::uint8_t ability_axes{0};
    float mouse_x{0.f};
    float mouse_y{0.f};
    std::uint8_t mouse_buttons{0};
};

struct AppContext {
    // Menus and text fields
    Screen screen_{Screen::Home};
    std::uint16_t game_port_{7777};
    float sim_accumulator_{0.f};

    std::string join_ipv4_buf_{"127.0.0.1"};
    std::string join_port_buf_{"7777"};
    JoinEditField join_edit_field_{JoinEditField::Ip};

    std::string options_port_buf_{"7777"};
    OptionsEditField options_edit_field_{OptionsEditField::UdpPort};
    std::string options_delay_ticks_buf_{"0"};
    std::string options_interp_ms_buf_{"88"};
    std::uint8_t stored_remote_delay_ticks_{0};       // copied to world_.input_delay_ticks_b at match start
    std::uint16_t stored_client_interp_delay_ms_{kDefaultClientInterpDelayMs};

    std::string feedback_line_{};
    bool feedback_error_{false};

    // Host listen server (same process as the game window)
    std::unique_ptr<zh::net::ListenHost> listen_server_;
    zh::net::host::HostGameplayRouter host_router_{};

    // Outbound client connection + prediction state
    zh::client::WanClientSession wan_{};

    Rectangle join_btn_bounds_{};
    Rectangle host_btn_bounds_{};
    Rectangle options_btn_bounds_{};

    bool match_running_{false};
    bool remote_client_{false};  // true when we joined someone else's host

    // Authoritative sim — only the host advances this
    zh::game::MatchWorld world_{};

    // Per-remote-slot input from the network (host side)
    std::array<bool, zh::kRemoteSlots> slot_active_{};
    std::array<void *, zh::kRemoteSlots> slot_peer_{};
    std::array<std::uint32_t, zh::kRemoteSlots> slot_last_seq_{};
    std::array<std::uint8_t, zh::kRemoteSlots> slot_move_{};
    std::array<std::uint8_t, zh::kRemoteSlots> slot_ability_{};
    std::array<float, zh::kRemoteSlots> slot_mouse_x_{};
    std::array<float, zh::kRemoteSlots> slot_mouse_y_{};
    std::array<std::uint8_t, zh::kRemoteSlots> slot_mouse_buttons_{};
    // When input_delay_ticks_b > 0, packets wait here until their exec tick.
    std::array<std::deque<zh::net::host::DelayedRemoteInput>, zh::kRemoteSlots> remote_input_schedule_{};
    std::array<std::uint8_t, zh::kRemoteSlots> remote_mouse_seen_{};  // for edge detect on clicks
    std::uint8_t occupied_slots_mask_{0};

    // Pre-match roster (host edits, clients mirror via PackedLobbyTeams)
    std::uint8_t lobby_host_team_{0};  // 0 = team A (west), 1 = team B
    std::uint8_t lobby_slot_team_b_mask_{0};
    std::uint8_t lobby_host_goalie_{0};
    std::uint8_t lobby_slot_goalie_mask_{0};

    // Client-side snapshot double-buffer for interp + prediction
    zh::PackedSnapshot snap_prev_{};
    zh::PackedSnapshot snap_new_{};
    bool have_two_snaps_{false};
    double snap_prev_time_{0.};
    double snap_new_time_{0.};

    // Client lobby UI mirrors the last PackedLobbyTeams from the host.
    std::uint8_t client_lobby_host_team_{0};
    std::uint8_t client_lobby_slot_team_mask_{0};
    std::uint8_t client_lobby_occ_mask_{0};
    std::uint8_t client_lobby_host_goalie_{0};
    std::uint8_t client_lobby_slot_goalie_mask_{0};

    char host_session_hint_[176]{};
    double host_session_hint_until_sec_{0.};

    Texture2D player_tex_team_a_{};
    Texture2D player_tex_team_b_{};
    Texture2D player_goalie_tex_team_a_{};
    Texture2D player_goalie_tex_team_b_{};
    bool player_tex_ready_{false};
    zh::ui::RinkSpriteSet rink_sprites_{};
    zh::ui::SkaterFxSystem skater_fx_{};
    zh::ui::GameSoundBank game_sounds_{};
    double last_puck_collision_sound_sec_{0.};
    double last_puck_metal_collision_sound_sec_{0.};
    std::uint32_t client_sound_prev_boost_os_{0};
    std::uint32_t client_sound_prev_one_timer_{0};
    std::uint32_t client_sound_prev_shield_{0};
    std::uint32_t host_sound_prev_one_timer_{0};
    std::uint32_t host_sound_prev_shield_{0};
    int sound_prev_faceoff_countdown_secs_{0};

    Camera2D playing_cam_{};
    bool playing_cam_ready_{false};
    float playing_cam_view_frac_{0.75f};
    float playing_cam_focus_x_{0.f};
    float playing_cam_focus_y_{0.f};
    float playing_cam_smooth_x_{0.f};
    float playing_cam_smooth_y_{0.f};

    void host_reset_session();
    void client_reset_session();
    void service_listen_peers_if_host();

    void poll_network();

    void sim_fixed_step(float dt);
    void sim_home(float dt);
    void sim_match(float dt);
    [[nodiscard]] std::uint8_t sample_input_bits() const;
    [[nodiscard]] std::uint8_t sample_ability_bits() const;
    [[nodiscard]] Rectangle playing_leave_bounds() const;
    [[nodiscard]] std::uint8_t sample_local_match_mouse(Rectangle leave,
                                                         float &mouse_x_out,
                                                         float &mouse_y_out);
    [[nodiscard]] LocalPlayingInputSample sample_local_playing_input();

    void render_frame(float interp_alpha, float frame_dt);

    void draw_home();
    void draw_join_form();
    void draw_host_lobby();
    void draw_options();
    void draw_client_lobby();
    void draw_playing(float interp_alpha, float frame_dt);

    void start_listen_server(std::uint16_t port);
    void stop_listen_server();

    [[nodiscard]] bool start_client_join(std::string const &ipv4, std::uint16_t port);
    void stop_client();

    [[nodiscard]] int run();

    void record_sent_client_input(zh::PackedClientInput const &in);

    void advance_client_local_predictor(float frame_dt);
    void advance_client_puck_prediction(float frame_dt);

    void broadcast_lobby_teams_if_hosting();
    void apply_match_start_spawn_from_lobby_teams();

    void load_player_team_textures();
    void unload_player_team_textures_if_any();
    void load_rink_texture();
    void unload_rink_texture_if_any();
    void load_game_sound_bank();
    void unload_game_sound_bank_if_any();
    void play_match_sounds(zh::game::MatchSoundEvents const &events) noexcept;
    void poll_client_local_sounds() noexcept;
    void poll_match_ambient_sounds() noexcept;
    void reset_playing_camera() noexcept;
    void apply_playing_camera_zoom() noexcept;
    void resolve_playing_camera_focus(float &focus_x, float &focus_y) const noexcept;
    void update_playing_camera(float focus_x, float focus_y, float frame_dt) noexcept;
    [[nodiscard]] Vector2 playing_screen_to_world(Vector2 screen) const noexcept;

    void load_persisted_user_settings();
    [[nodiscard]] bool persist_user_settings() const;
};

}  // namespace zh::detail
