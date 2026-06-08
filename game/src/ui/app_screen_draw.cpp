// Per-screen Raylib draw paths for menus, lobby, and playing HUD (large TU).

#include "detail/app_context.hpp"
#include "zh/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <raylib.h>

#include "zh/client/client_interp_frame.hpp"
#include "zh/game/constants.hpp"
#include "zh/game/lobby_caps.hpp"
#include "zh/game/match_world.hpp"
#include "zh/game/puck_physics.hpp"
#include "zh/game/skater_physics.hpp"
#include "zh/ui/draw_widgets.hpp"
#include "zh/ui/form_parse.hpp"
#include "zh/ui/hit_test.hpp"
#include "zh/ui/skater_fx.hpp"

namespace zh::detail {

using zh::client::ClientInterpFrame;

namespace {

using zh::ui::parse_delay_ticks_string;
using zh::ui::parse_interp_delay_ms_string;
using zh::ui::parse_udp_port_string;
using zh::ui::point_in;

constexpr int kUiFontSize = 24;
constexpr float kPlayingHudInsetY = 10.f;
constexpr int kPlayingHudLeftX = 48;
constexpr Color kPlayingHudNetCol = Color{178, 195, 212, 255};
constexpr Color kPlayingHudDebugCol = Color{130, 146, 164, 255};
constexpr float kPlayerSpriteDrawPx = 32.f;

using namespace zh::game;

void infer_draw_skater_axes(float const older_px,
                            float const older_py,
                            float const newer_px,
                            float const newer_py,
                            float const stored_face_x,
                            float const stored_face_y,
                            float &out_vx,
                            float &out_vy,
                            float &out_face_x,
                            float &out_face_y) noexcept {
    out_face_x = stored_face_x;
    out_face_y = stored_face_y;
    out_vx = (newer_px - older_px) * 60.f;
    out_vy = (newer_py - older_py) * 60.f;
    if (std::hypot(out_vx, out_vy) < 1e-3f) {
        out_vx = stored_face_x * kSkaterMoveFacingSpeed;
        out_vy = stored_face_y * kSkaterMoveFacingSpeed;
    }
    set_facing_toward(older_px, older_py, newer_px, newer_py, out_face_x, out_face_y);
}

void draw_player_stick_hitbox(float const px,
                              float const py,
                              float const vx,
                              float const vy,
                              float const face_x,
                              float const face_y,
                              bool const highlight) noexcept {
    Color const fill =
        highlight ? Color{90, 200, 255, 48} : Color{180, 180, 190, 34};
    Color const stroke =
        highlight ? Color{130, 220, 255, 190} : Color{200, 200, 210, 120};
    zh::ui::draw_stick_hitbox(px, py, vx, vy, face_x, face_y, fill, stroke);
}

void draw_faceoff_countdown_overlay(int const countdown_secs) noexcept {
    float const sw = static_cast<float>(GetScreenWidth());
    float const sh = static_cast<float>(GetScreenHeight());
    Vector2 const center{sw * 0.5f, sh * 0.5f};

    char label[8];
    std::snprintf(label, sizeof(label), "%d", std::max(1, countdown_secs));
    int const fs = 112;
    int const tw = MeasureText(label, fs);
    float const circle_r = std::max(88.f, static_cast<float>(tw) * 0.72f + 48.f);

    DrawCircleV(center, circle_r + 6.f, Color{8, 10, 16, 140});
    DrawCircleV(center, circle_r, Color{18, 22, 32, 210});
    DrawRing(center, circle_r - 5.f, circle_r, 0.f, 360.f, 72, Color{255, 220, 120, 255});
    DrawRing(center, circle_r - 9.f, circle_r - 6.f, 0.f, 360.f, 72, Color{255, 220, 120, 90});

    DrawText(label, static_cast<int>(center.x - static_cast<float>(tw) * 0.5f),
             static_cast<int>(center.y - static_cast<float>(fs) * 0.5f), fs, GOLD);
}

} // namespace

// --- Home menu ---
void AppContext::draw_home() {
    // Re-layout every frame: first framebuffer/DPI callbacks can change logical screen size after InitWindow,
    // and the window may be resized; stale bounds break hit testing.
    float const cw = static_cast<float>(GetScreenWidth());
    float const ch = static_cast<float>(GetScreenHeight());
    float const bw = cw * 0.35f;
    float const bh = 54.f;
    float const bx = cw * 0.5f - bw * 0.5f;
    float const gy = ch * 0.42f;
    join_btn_bounds_ = {bx, gy, bw, bh};
    host_btn_bounds_ = {bx, gy + bh + 24.f, bw, bh};
    options_btn_bounds_ = {bx, gy + 2.f * (bh + 24.f), bw, bh};

    int const scr_w = GetScreenWidth();
    int const scr_h = GetScreenHeight();
    char const title[] = "ZH Standalone";
    int const tw = MeasureText(title, 48);
    DrawText(title,
             static_cast<int>((static_cast<float>(scr_w) - static_cast<float>(tw)) * 0.5f),
             static_cast<int>(static_cast<float>(scr_h) * 0.20f), 48, WHITE);

    Vector2 const mp = GetMousePosition();
    bool const hover_join = point_in(join_btn_bounds_, mp);
    bool const hover_host = point_in(host_btn_bounds_, mp);
    bool const hover_opts = point_in(options_btn_bounds_, mp);

    zh::ui::draw_centered_rect_button(join_btn_bounds_, Color{60, 120, 200, 255},
                               Color{120, 180, 255, 255}, "Join", kUiFontSize, hover_join);

    zh::ui::draw_centered_rect_button(host_btn_bounds_, Color{60, 160, 90, 255},
                               Color{130, 220, 150, 255}, "Host", kUiFontSize, hover_host);

    zh::ui::draw_centered_rect_button(options_btn_bounds_, Color{90, 90, 110, 255},
                               Color{180, 180, 200, 255}, "Options", kUiFontSize, hover_opts);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (point_in(join_btn_bounds_, mp)) {
            feedback_line_.clear();
            feedback_error_ = false;
            join_edit_field_ = JoinEditField::Ip;
            screen_ = Screen::JoinForm;
        } else if (point_in(host_btn_bounds_, mp)) {
            host_reset_session();
            client_reset_session();
            start_listen_server(game_port_);
            if ((listen_server_ != nullptr) && listen_server_->valid()) {
                screen_ = Screen::HostLobby;
                std::printf("[zh] Listening on UDP port %u (ENet).\n",
                           static_cast<unsigned>(game_port_));
            } else {
                std::fprintf(stderr, "[zh] Failed to bind UDP port %u.\n",
                           static_cast<unsigned>(game_port_));
            }
        } else if (point_in(options_btn_bounds_, mp)) {
            char pb[16]{};
            (void)std::snprintf(pb, sizeof(pb), "%u",
                                static_cast<unsigned>(game_port_));
            options_port_buf_ = pb;
            options_edit_field_ = OptionsEditField::UdpPort;
            char db[8]{};
            (void)std::snprintf(db, sizeof(db), "%u",
                                static_cast<unsigned>(stored_remote_delay_ticks_));
            options_delay_ticks_buf_ = db;
            char ib[8]{};
            (void)std::snprintf(ib, sizeof(ib), "%u",
                                static_cast<unsigned>(stored_client_interp_delay_ms_));
            options_interp_ms_buf_ = ib;
            feedback_line_.clear();
            feedback_error_ = false;
            screen_ = Screen::Options;
        }
    }
}


// --- Join form (IPv4 + port, connect to host) ---
void AppContext::draw_join_form() {
    DrawText("Join game", 48, 48, 36, WHITE);
    DrawText("Host IPv4 (ASCII)", 48, 112, kUiFontSize, LIGHTGRAY);
    DrawText("UDP port", 48, 200, kUiFontSize, LIGHTGRAY);

    Rectangle field_ip{48.f, 146.f, 520.f, 44.f};
    Rectangle field_port{48.f, 234.f, 220.f, 44.f};
    Vector2 mp = GetMousePosition();
    bool const hover_ip = point_in(field_ip, mp);
    bool const hover_port = point_in(field_port, mp);
    bool const ip_focus = (join_edit_field_ == JoinEditField::Ip);
    bool const port_focus = (join_edit_field_ == JoinEditField::Port);

    auto const draw_text_field =
        [](Rectangle rect, bool focus, bool hover, std::string const &text, int px) noexcept {
            Color bg = Color{30, 35, 55, 255};
            if (focus) {
                bg = Color{40, 45, 70, 255};
            }
            if (!focus && hover) {
                bg = ColorBrightness(bg, 0.14f);
            }
            DrawRectangleRec(rect, bg);
            Color border{};
            float th = 2.f;
            if (focus) {
                border = SKYBLUE;
                th = 3.f;
            } else if (hover) {
                border = Color{120, 200, 255, 255};
                th = 2.5f;
            } else {
                border = DARKGRAY;
            }
            DrawRectangleLinesEx(rect, th, border);
            DrawText(text.c_str(), static_cast<int>(rect.x + 12.f),
                     static_cast<int>(rect.y + 10.f), px, RAYWHITE);
        };

    draw_text_field(field_ip, ip_focus, hover_ip, join_ipv4_buf_, kUiFontSize);
    draw_text_field(field_port, port_focus, hover_port, join_port_buf_, kUiFontSize);

    Rectangle back{48.f, 330.f, 180.f, 48.f};
    Rectangle conn{260.f, 330.f, 220.f, 48.f};
    bool const hover_back = point_in(back, mp);
    bool const hover_conn = point_in(conn, mp);

    if (!feedback_line_.empty()) {
        DrawText(feedback_line_.c_str(),
                 48,
                 static_cast<int>(back.y - 36.f),
                 kUiFontSize - 2,
                 feedback_error_ ? Color{235, 100, 100, 255} : Color{230, 200, 120, 255});
    }

    DrawRectangleRec(back, hover_back ? ColorBrightness(MAROON, 0.22f) : MAROON);
    DrawRectangleRec(conn, hover_conn ? ColorBrightness(DARKBLUE, 0.22f) : DARKBLUE);
    DrawRectangleLinesEx(back, hover_back ? 3.5f : 2.f, hover_back ? GOLD : RAYWHITE);
    DrawRectangleLinesEx(conn, hover_conn ? 3.5f : 2.f, hover_conn ? GOLD : RAYWHITE);

    int const lw_back = MeasureText("Back", kUiFontSize);
    Color const back_txt = hover_back ? GOLD : WHITE;
    DrawText(
        "Back",
        static_cast<int>(back.x + (static_cast<float>(back.width - lw_back)) * 0.5f),
        static_cast<int>(back.y + (static_cast<float>(back.height - kUiFontSize)) * 0.5f),
        kUiFontSize, back_txt);

    int const lw_cn = MeasureText("Connect", kUiFontSize);
    Color const conn_txt = hover_conn ? GOLD : WHITE;
    DrawText(
        "Connect",
        static_cast<int>(conn.x + (static_cast<float>(conn.width - lw_cn)) * 0.5f),
        static_cast<int>(
            conn.y + (static_cast<float>(conn.height - kUiFontSize)) * 0.5f),
        kUiFontSize, conn_txt);

    if (IsKeyPressed(KEY_TAB)) {
        join_edit_field_ = ip_focus ? JoinEditField::Port : JoinEditField::Ip;
    }

    if (ip_focus) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 126) {
                if (join_ipv4_buf_.size() < 45U) {
                    join_ipv4_buf_.push_back(static_cast<char>(key));
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && (!join_ipv4_buf_.empty())) {
            join_ipv4_buf_.pop_back();
        }
    } else if (port_focus) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= static_cast<int>('0') && key <= static_cast<int>('9')) {
                if (join_port_buf_.size() < 5U) {
                    join_port_buf_.push_back(static_cast<char>(key));
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && (!join_port_buf_.empty())) {
            join_port_buf_.pop_back();
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        mp = GetMousePosition();
        if (point_in(field_ip, mp)) {
            join_edit_field_ = JoinEditField::Ip;
        } else if (point_in(field_port, mp)) {
            join_edit_field_ = JoinEditField::Port;
        } else if (point_in(back, mp)) {
            feedback_line_.clear();
            feedback_error_ = false;
            screen_ = Screen::Home;
            return;
        } else if (point_in(conn, mp)) {
            std::uint16_t connect_port = 0;
            if (!parse_udp_port_string(join_port_buf_, connect_port)) {
                feedback_error_ = true;
                feedback_line_ = "Invalid UDP port — use digits 1-65535.";
                return;
            }

            std::string ip = join_ipv4_buf_;
            if (ip.empty()) {
                ip = "127.0.0.1";
            }
            stop_listen_server();
            client_reset_session();
            if (start_client_join(ip, connect_port)) {
                remote_client_ = true;
                screen_ = Screen::ClientLobby;
                feedback_line_.clear();
                feedback_error_ = false;
                std::printf("[zh] Connecting to %s:%u ...\n", ip.c_str(),
                           static_cast<unsigned>(connect_port));
            } else {
                feedback_error_ = true;
                feedback_line_ = "Connect failed — is host up on that IP:port?";
                std::fprintf(stderr,
                             "[zh] connect_to failed for '%s' UDP %u "
                             "(is the host listening?)\n",
                             ip.c_str(), static_cast<unsigned>(connect_port));
            }
        }
    }

    DrawText(
        "Tip: Tab switches IP/port fields — you can join during the host lobby; first snapshot starts "
        "the match.",
        48,
        GetScreenHeight() - 88, kUiFontSize - 6, LIGHTGRAY);
}


// --- Host lobby (team/goalie picks, Start match, listen server) ---
void AppContext::draw_host_lobby() {
    DrawText("Host lobby — arcade teams & roles", 48, 38, 36, WHITE);

    char line[160];
    std::snprintf(line, sizeof(line),
                  "Listening on UDP %u — guests join before Start.",
                  static_cast<unsigned>(game_port_));
    DrawText(line, 48, 74, kUiFontSize - 6, LIGHTGRAY);
    LobbySideCount const cnt_w =
        lobby_side_count(false, lobby_host_team_, lobby_host_goalie_,
                         lobby_slot_team_b_mask_, lobby_slot_goalie_mask_, occupied_slots_mask_);
    LobbySideCount const cnt_e =
        lobby_side_count(true, lobby_host_team_, lobby_host_goalie_,
                         lobby_slot_team_b_mask_, lobby_slot_goalie_mask_, occupied_slots_mask_);
    char cap_vis[172];
    std::snprintf(cap_vis, sizeof(cap_vis),
                  "Caps west/east ≤%u & ≤%u GK (%u SK + GK plan). Counts→ west %usk+%ugk | east "
                  "%usk+%ugk",
                  static_cast<unsigned>(kLobbyMaxPlayersPerSide),
                  static_cast<unsigned>(kLobbyMaxGoaliesPerSide),
                  static_cast<unsigned>(kLobbyMaxPlayersPerSide - kLobbyMaxGoaliesPerSide),
                  cnt_w.total - cnt_w.goalies, cnt_w.goalies,
                  cnt_e.total - cnt_e.goalies, cnt_e.goalies);
    DrawText(cap_vis, 48, 96, kUiFontSize - 10, Color{135, 150, 170, 255});
    DrawText("Main card: toggle team ⇄ rival column · right pill (SK ⇄ GK): role (goalie limit enforced).",
             48, 118, kUiFontSize - 10, Color{148, 152, 170, 255});

    constexpr float row_h = 52.f;
    constexpr float row_gap = 11.f;
    constexpr float lx = 48.f;
    constexpr float lw = 528.f;
    constexpr float rx = 628.f;
    constexpr float role_pill_w = 86.f;

    struct HitZone {
        Rectangle bounds{};
        bool role_zone{};
        bool is_host{};
        unsigned slot_ix{};  // Ignored when is_host.
    };

    DrawText("Team A", static_cast<int>(lx), static_cast<int>(148), kUiFontSize + 4,
             Color{120, 200, 255, 255});
    DrawText("Team B", static_cast<int>(rx), static_cast<int>(148), kUiFontSize + 4,
             Color{255, 148, 120, 255});
    DrawLineEx(Vector2{606.f, 142.f}, Vector2{606.f, 648.f}, 3.f,
               Color{60, 64, 90, 200});

    Vector2 mp = GetMousePosition();
    float y_left = 188.f;
    float y_right = 188.f;

    std::array<HitZone, 26> hz{};
    std::size_t hz_n = 0;
    auto push_hit = [&](Rectangle r, bool role_toggle, bool is_host_pl, unsigned rem_slot) noexcept {
        if (hz_n < hz.size()) {
            hz[hz_n].bounds = r;
            hz[hz_n].role_zone = role_toggle;
            hz[hz_n].is_host = is_host_pl;
            hz[hz_n].slot_ix = rem_slot;
            ++hz_n;
        }
    };

    auto emit_row_left = [&](char const *label, char const *hint, bool host_pl,
                             unsigned remote_slot_ix, bool is_goalie_player) noexcept {
        Rectangle outer{lx, y_left, lw, row_h};
        bool const hot_outer = CheckCollisionPointRec(mp, outer);
        Color fill_base = Color{28, 48, 86, 255};
        if (hot_outer) {
            fill_base = ColorBrightness(fill_base, 0.13f);
        }
        DrawRectangleRec(outer, fill_base);
        DrawRectangleLinesEx(outer, hot_outer ? 2.95f : 2.f,
                             Color{110, 180, 235, 255});

        Rectangle r_team{outer.x + 7.f, outer.y + 5.f,
                         outer.width - role_pill_w - 21.f, outer.height - 10.f};
        Rectangle r_role{outer.x + outer.width - role_pill_w - 10.f,
                         outer.y + 8.f,
                         role_pill_w - 6.f,
                         outer.height - 16.f};
        DrawText(label, static_cast<int>(r_team.x + 4.f),
                 static_cast<int>(r_team.y + 6.f),
                 kUiFontSize - 2, WHITE);
        DrawText(hint, static_cast<int>(r_team.x + 4.f),
                 static_cast<int>(r_team.y + 28.f),
                 kUiFontSize - 11,
                 Color{180, 205, 230, 255});

        DrawRectangleRec(r_role, is_goalie_player ? Color{190, 200, 80, 235}
                                                   : Color{75, 100, 150, 220});
        DrawRectangleLinesEx(r_role, CheckCollisionPointRec(mp, r_role) ? 2.8f : 2.f,
                             Color{220, 230, 248, 255});
        DrawText(is_goalie_player ? " GK " : " SK ",
                 static_cast<int>(r_role.x + 18.f),
                 static_cast<int>(r_role.y + r_role.height * 0.5f - 10.f),
                 kUiFontSize - 6, Color{18, 20, 32, 255});

        push_hit(r_team, false, host_pl, remote_slot_ix);
        push_hit(r_role, true, host_pl, remote_slot_ix);

        y_left += row_h + row_gap;
    };

    auto emit_row_right = [&](char const *label, char const *hint, bool host_pl,
                              unsigned remote_slot_ix, bool is_goalie_player) noexcept {
        Rectangle outer{rx, y_right, lw, row_h};
        bool const hot_outer = CheckCollisionPointRec(mp, outer);
        Color fill_base = Color{74, 32, 40, 255};
        if (hot_outer) {
            fill_base = ColorBrightness(fill_base, 0.12f);
        }
        DrawRectangleRec(outer, fill_base);
        DrawRectangleLinesEx(outer, hot_outer ? 2.95f : 2.f,
                             Color{230, 120, 100, 255});

        Rectangle r_team{outer.x + 7.f, outer.y + 5.f,
                         outer.width - role_pill_w - 21.f, outer.height - 10.f};
        Rectangle r_role{outer.x + outer.width - role_pill_w - 10.f,
                         outer.y + 8.f,
                         role_pill_w - 6.f,
                         outer.height - 16.f};
        DrawText(label, static_cast<int>(r_team.x + 4.f),
                 static_cast<int>(r_team.y + 6.f),
                 kUiFontSize - 2, WHITE);
        DrawText(hint, static_cast<int>(r_team.x + 4.f),
                 static_cast<int>(r_team.y + 28.f),
                 kUiFontSize - 11,
                 Color{240, 200, 195, 255});

        DrawRectangleRec(r_role, is_goalie_player ? Color{190, 200, 80, 235}
                                                   : Color{130, 60, 70, 235});
        DrawRectangleLinesEx(r_role, CheckCollisionPointRec(mp, r_role) ? 2.8f : 2.f,
                             Color{255, 220, 200, 255});
        DrawText(is_goalie_player ? " GK " : " SK ",
                 static_cast<int>(r_role.x + 18.f),
                 static_cast<int>(r_role.y + r_role.height * 0.5f - 10.f),
                 kUiFontSize - 6, Color{18, 20, 32, 255});

        push_hit(r_team, false, host_pl, remote_slot_ix);
        push_hit(r_role, true, host_pl, remote_slot_ix);

        y_right += row_h + row_gap;
    };

    bool const host_b = lobby_host_team_ != 0;
    bool const host_goalie_now = lobby_host_goalie_ != 0;
    char host_label[96];
    std::snprintf(host_label, sizeof(host_label),
                  "Host (you)%s", host_goalie_now ? "  ·  Goalie roster" : "  ·  Skater roster");
    if (!host_b) {
        emit_row_left(host_label, "Main area: move to east · SK/GK toggles goalie cap",
                      true,
                      0U,
                      host_goalie_now);
    } else {
        emit_row_right(host_label, "Main area: move to west · SK/GK toggles goalie cap",
                       true,
                       0U,
                       host_goalie_now);
    }

    char gt[88];
    char gh[152];
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if (!slot_active_[i]) {
            continue;
        }
        bool const guest_team_b =
            (lobby_slot_team_b_mask_ & static_cast<std::uint8_t>(1U << i)) != 0;
        bool const guest_g =
            (lobby_slot_goalie_mask_ & static_cast<std::uint8_t>(1U << i)) != 0;
        std::snprintf(gt, sizeof(gt), "Guest · slot %u%s",
                      static_cast<unsigned>(i),
                      guest_g ? "  ·  GK slot" : "  ·  Skater slot");
        std::snprintf(gh, sizeof(gh),
                      "Joined via UDP/ENet — host PC controls team & GK/SK labeling.");
        unsigned const ix = static_cast<unsigned>(i);
        if (!guest_team_b) {
            emit_row_left(gt, gh, false, ix, guest_g);
        } else {
            emit_row_right(gt, gh, false, ix, guest_g);
        }
    }

    float roster_bottom = (y_left > y_right ? y_left : y_right) + row_gap + 6.f;

    if (!feedback_line_.empty()) {
        DrawText(feedback_line_.c_str(),
                 48,
                 static_cast<int>(roster_bottom - 34.f),
                 kUiFontSize - 8,
                 feedback_error_ ? Color{240, 120, 120, 255} : Color{220, 205, 120, 255});
    }

    Rectangle start{48.f, roster_bottom + 44.f, 296.f, 52.f};
    bool const hover_start = CheckCollisionPointRec(mp, start);
    DrawRectangleRec(start, hover_start ? ColorBrightness(DARKGREEN, 0.25f) : DARKGREEN);
    DrawRectangleLinesEx(start, hover_start ? 4.f : 3.f,
                         hover_start ? GOLD : RAYWHITE);
    DrawText("Start match", static_cast<int>(start.x + 44.f),
             static_cast<int>(start.y + 14.f), kUiFontSize,
             hover_start ? GOLD : WHITE);

    Rectangle back{380.f, static_cast<float>(GetScreenHeight() - 138), 280.f, 52.f};
    bool const hover_stop = CheckCollisionPointRec(mp, back);
    DrawRectangleRec(back, hover_stop ? ColorBrightness(MAROON, 0.22f) : MAROON);
    DrawRectangleLinesEx(back, hover_stop ? 4.f : 3.f, hover_stop ? GOLD : RAYWHITE);
    DrawText("Stop hosting",
             static_cast<int>(back.x + 44.f), static_cast<int>(back.y + 14.f), kUiFontSize,
             hover_stop ? GOLD : WHITE);

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }

    for (std::size_t zi = 0; zi < hz_n; ++zi) {
        HitZone const &z = hz[zi];
        if (!CheckCollisionPointRec(mp, z.bounds)) {
            continue;
        }
        if (z.role_zone) {
            if (z.is_host) {
                std::uint8_t const ng =
                    static_cast<std::uint8_t>(lobby_host_goalie_ ? 0 : 1);
                if (!(lobby_both_teams_within_caps(lobby_host_team_, ng,
                                                     lobby_slot_team_b_mask_,
                                                     lobby_slot_goalie_mask_,
                                                     occupied_slots_mask_))) {
                    feedback_error_ = true;
                    feedback_line_ =
                        "That GK/SK toggle breaks plan caps — max 4/side & 1 GK.";
                } else {
                    lobby_host_goalie_ = ng;
                    feedback_error_ = false;
                    feedback_line_.clear();
                    broadcast_lobby_teams_if_hosting();
                }
            } else if (z.slot_ix < static_cast<unsigned>(kRemoteSlots) &&
                       slot_active_[z.slot_ix]) {
                std::uint8_t const bit =
                    static_cast<std::uint8_t>(static_cast<unsigned>(1U << z.slot_ix));
                std::uint8_t nmask =
                    static_cast<std::uint8_t>(lobby_slot_goalie_mask_ ^ bit);
                if (!(lobby_both_teams_within_caps(lobby_host_team_, lobby_host_goalie_,
                                                     lobby_slot_team_b_mask_,
                                                     nmask,
                                                     occupied_slots_mask_))) {
                    feedback_error_ = true;
                    feedback_line_ = "GK limit on that roster — toggle blocked.";
                } else {
                    lobby_slot_goalie_mask_ = nmask;
                    feedback_error_ = false;
                    feedback_line_.clear();
                    broadcast_lobby_teams_if_hosting();
                }
            }
            return;
        }

        bool ok_to_commit = false;
        if (z.is_host) {
            std::uint8_t const nh =
                lobby_host_team_ ? 0U : 1U;
            if (lobby_both_teams_within_caps(nh, lobby_host_goalie_,
                                               lobby_slot_team_b_mask_,
                                               lobby_slot_goalie_mask_,
                                               occupied_slots_mask_)) {
                lobby_host_team_ = nh;
                ok_to_commit = true;
            } else {
                feedback_error_ = true;
                feedback_line_ =
                    "Destination roster full (max 4) — shuffle someone away first.";
            }
        } else if (z.slot_ix < static_cast<unsigned>(kRemoteSlots) &&
                   slot_active_[z.slot_ix]) {
            std::uint8_t const bit_mask =
                static_cast<std::uint8_t>(static_cast<unsigned>(1U << z.slot_ix));
            std::uint8_t nt_mask =
                static_cast<std::uint8_t>(lobby_slot_team_b_mask_ ^ bit_mask);
            if (lobby_both_teams_within_caps(lobby_host_team_, lobby_host_goalie_,
                                               nt_mask,
                                               lobby_slot_goalie_mask_,
                                               occupied_slots_mask_)) {
                lobby_slot_team_b_mask_ = nt_mask;
                ok_to_commit = true;
            } else {
                feedback_error_ = true;
                feedback_line_ = "Opposite roster capped at 4 — move frees a seat first.";
            }
        }

        if (ok_to_commit) {
            feedback_error_ = false;
            feedback_line_.clear();
            broadcast_lobby_teams_if_hosting();
        }
        return;
    }

    if (CheckCollisionPointRec(mp, start)) {
        if (!(lobby_both_teams_within_caps(lobby_host_team_, lobby_host_goalie_,
                                           lobby_slot_team_b_mask_,
                                           lobby_slot_goalie_mask_,
                                           occupied_slots_mask_))) {
            feedback_error_ = true;
            feedback_line_ =
                "Roster violates plan caps (≤4 rostered / ≤1 GK per bench). Rearrange.";
            return;
        }
        broadcast_lobby_teams_if_hosting();
        for (auto &dq : remote_input_schedule_) {
            dq.clear();
        }
        world_.host_ability_prev = 0;
        world_.host_boost_active_ticks = 0;
        world_.host_boost_cd_ticks = {};
        world_.host_boost_overspeed_ticks = 0;
        world_.host_slide_cd_ticks = 0;
        world_.host_slide_decel_ticks = 0;
        world_.host_slide_carry_speed = 0.f;
        world_.host_brake_cd_ticks = 0;
        world_.host_steal_cd_ticks = 0;
        world_.host_pickup_block_ticks = 0;
        world_.host_one_timer_ticks = 0;
        world_.host_one_timer_cd_ticks = 0;
        for (std::size_t bi = 0; bi < kRemoteSlots; ++bi) {
            world_.slot_ability_prev[bi] = 0;
            world_.slot_boost_active_ticks[bi] = 0;
            world_.slot_boost_cd_ticks[bi] = {};
            world_.slot_boost_overspeed_ticks[bi] = 0;
            world_.slot_slide_cd_ticks[bi] = 0;
            world_.slot_slide_decel_ticks[bi] = 0;
            world_.slot_slide_carry_speed[bi] = 0.f;
            world_.slot_brake_cd_ticks[bi] = 0;
            world_.slot_steal_cd_ticks[bi] = 0;
            world_.slot_pickup_block_ticks[bi] = 0;
            world_.slot_one_timer_ticks[bi] = 0;
            world_.slot_one_timer_cd_ticks[bi] = 0;
        }
        world_.input_delay_ticks_b = stored_remote_delay_ticks_;
        world_.server_tick = 0;
        apply_match_start_spawn_from_lobby_teams();
        world_.begin_match();
        reset_playing_camera();
        match_running_ = true;
        feedback_error_ = false;
        feedback_line_.clear();
        screen_ = Screen::Playing;
        std::printf("[zh] Match started (UDP %u, WAN delay B=%u).\n",
                    static_cast<unsigned>(game_port_),
                    static_cast<unsigned>(world_.input_delay_ticks_b));
        return;
    }
    if (CheckCollisionPointRec(mp, back)) {
        stop_listen_server();
        host_reset_session();
        feedback_line_.clear();
        feedback_error_ = false;
        screen_ = Screen::Home;
    }
}

// --- Client lobby (read-only roster until first snapshot) ---
void AppContext::draw_client_lobby() {
    DrawText("Lobby", 48, 40, 36, WHITE);
    DrawText("Waiting for the host to start the match.", 48, 82, kUiFontSize - 2, LIGHTGRAY);

    struct RowGuest {
        bool is_host{};
        unsigned slot_ix{0};  // For remote rows only (ignored when is_host).
        bool highlight_you{};
    };
    constexpr float row_h = 50.f;
    constexpr float row_gap = 10.f;
    constexpr float lx = 48.f;
    constexpr float lw = 528.f;
    constexpr float rx = 628.f;

    DrawText("Team A", static_cast<int>(lx), static_cast<int>(134), kUiFontSize + 4,
             Color{120, 200, 255, 255});
    DrawText("Team B", static_cast<int>(rx), static_cast<int>(134), kUiFontSize + 4,
             Color{255, 148, 120, 255});
    DrawLineEx(Vector2{606.f, 130.f}, Vector2{606.f, 628.f}, 3.f,
               Color{60, 64, 90, 200});

    Vector2 const mp = GetMousePosition();

    float y_left = 174.f;
    float y_right = 174.f;

    auto draw_left_readonly = [&](char const *title, char const *sub, RowGuest rg) noexcept {
        Rectangle rect{lx, y_left, lw, row_h};
        Color fill =
            rg.highlight_you ? Color{48, 60, 100, 255} : Color{28, 48, 86, 255};
        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, rg.highlight_you ? 3.4f : 2.f,
                             rg.highlight_you ? GOLD : Color{110, 180, 235, 255});
        DrawText(title, static_cast<int>(rect.x + 14.f),
                 static_cast<int>(rect.y + 8.f), kUiFontSize - 2, WHITE);
        DrawText(sub, static_cast<int>(rect.x + 14.f),
                 static_cast<int>(rect.y + 30.f), kUiFontSize - 11,
                 Color{180, 200, 220, 255});
        y_left += row_h + row_gap;
    };

    auto draw_right_readonly = [&](char const *title, char const *sub, RowGuest rg) noexcept {
        Rectangle rect{rx, y_right, lw, row_h};
        Color fill =
            rg.highlight_you ? Color{86, 50, 40, 255} : Color{74, 32, 40, 255};
        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, rg.highlight_you ? 3.4f : 2.f,
                             rg.highlight_you ? GOLD : Color{230, 120, 100, 255});
        DrawText(title, static_cast<int>(rect.x + 14.f),
                 static_cast<int>(rect.y + 8.f), kUiFontSize - 2, WHITE);
        DrawText(sub, static_cast<int>(rect.x + 14.f),
                 static_cast<int>(rect.y + 30.f), kUiFontSize - 11,
                 Color{220, 185, 180, 255});
        y_right += row_h + row_gap;
    };

    bool const host_team_b = client_lobby_host_team_ != 0;
    char host_sub_line[176];
    std::snprintf(host_sub_line, sizeof(host_sub_line), "Match starts via host · role tag: %s",
                  (client_lobby_host_goalie_ != 0) ? "GK roster" : "SK roster");
    if (!host_team_b) {
        draw_left_readonly("Host (listen-server)",
                           host_sub_line,
                           RowGuest{.is_host = true});
    } else {
        draw_right_readonly(
            "Host (listen-server)",
            host_sub_line,
            RowGuest{.is_host = true});
    }

    char title[96];
    char sub[144];
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if ((client_lobby_occ_mask_ & static_cast<std::uint8_t>(1U << i)) == 0) {
            continue;
        }
        bool const guest_team_b =
            (client_lobby_slot_team_mask_ & static_cast<std::uint8_t>(1U << i)) != 0;
        bool const guest_goalie =
            (client_lobby_slot_goalie_mask_ & static_cast<std::uint8_t>(1U << i)) != 0;

        RowGuest rg{.is_host = false,
                   .slot_ix = static_cast<unsigned>(i),
                   .highlight_you = false};
        if (wan_.my_slot != 255 && wan_.my_slot < kRemoteSlots &&
            static_cast<std::size_t>(wan_.my_slot) == i) {
            rg.highlight_you = true;
        }

        if (rg.highlight_you) {
            std::snprintf(title, sizeof(title), "You · slot %u · %s", static_cast<unsigned>(i),
                          guest_goalie ? "GK tag" : "SK tag");
            std::snprintf(sub, sizeof(sub),
                          "%s labelled — snaps enable gameplay.", guest_goalie ? "Goalie" : "Skater");
        } else {
            std::snprintf(title, sizeof(title), "Guest · slot %u · %s", static_cast<unsigned>(i),
                          guest_goalie ? "GK tag" : "SK tag");
            std::snprintf(sub, sizeof(sub),
                          "Host controls arcade bench before Start.");
        }
        if (!guest_team_b) {
            draw_left_readonly(title, sub, rg);
        } else {
            draw_right_readonly(title, sub, rg);
        }
    }

    float roster_bottom =
        ((y_left > y_right ? y_left : y_right)) + row_gap + 44.f;

    char slot_line[112];
    if (wan_.my_slot < kRemoteSlots) {
        std::snprintf(slot_line, sizeof(slot_line),
                      "You are mapped to remote slot %u (read-only roster — host rearranges teams).",
                      static_cast<unsigned>(wan_.my_slot));
    } else {
        std::snprintf(slot_line, sizeof(slot_line),
                      "(Slot assignment arriving — handshake in progress)");
    }
    DrawText(slot_line, 48, static_cast<int>(roster_bottom) - 38, kUiFontSize - 8,
             Color{150, 164, 180, 255});

    Rectangle cancel{48.f, static_cast<float>(GetScreenHeight() - 140), 296.f, 52.f};
    bool hover_cancel = CheckCollisionPointRec(mp, cancel);
    DrawRectangleRec(cancel,
                     hover_cancel ? ColorBrightness(MAROON, 0.22f) : MAROON);
    DrawRectangleLinesEx(cancel, hover_cancel ? 3.8f : 2.f,
                         hover_cancel ? GOLD : RAYWHITE);
    DrawText("Disconnect", static_cast<int>(cancel.x + 56.f),
             static_cast<int>(cancel.y + 14.f), kUiFontSize,
             hover_cancel ? GOLD : WHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover_cancel) {
        stop_client();
        client_reset_session();
        remote_client_ = false;
        screen_ = Screen::Home;
    }
}

// --- Options (default port, WAN delay B, client interp lag) ---
void AppContext::draw_options() {
    DrawText("Options", 48, 48, 36, WHITE);
    DrawText("Network", 48, 86, kUiFontSize, SKYBLUE);
    DrawText("Default UDP port (Host bind & Join form default)", 48, 118, kUiFontSize - 2,
             LIGHTGRAY);

    Vector2 mp = GetMousePosition();
    bool const udp_focus = options_edit_field_ == OptionsEditField::UdpPort;
    bool const delay_focus = options_edit_field_ == OptionsEditField::RemoteInputDelayTicks;
    bool const interp_focus = options_edit_field_ == OptionsEditField::ClientInterpLagMs;

    Rectangle field_port{48.f, 146.f, 220.f, 44.f};
    bool const hover_port = point_in(field_port, mp);

    Color port_bg = udp_focus ? Color{40, 45, 70, 255} : Color{30, 35, 55, 255};
    if ((!udp_focus) && hover_port) {
        port_bg = ColorBrightness(port_bg, 0.14f);
    }
    DrawRectangleRec(field_port, port_bg);
    Color port_br{};
    float port_thick = 2.f;
    if (udp_focus) {
        port_br = SKYBLUE;
        port_thick = 3.f;
    } else if (hover_port) {
        port_br = Color{120, 200, 255, 255};
        port_thick = 2.5f;
    } else {
        port_br = DARKGRAY;
    }
    DrawRectangleLinesEx(field_port, port_thick, port_br);
    DrawText(options_port_buf_.c_str(),
             static_cast<int>(field_port.x + 12.f),
             static_cast<int>(field_port.y + 10.f),
             kUiFontSize,
             RAYWHITE);

    DrawText("WAN remote input delay B (sim ticks)", 48, 210, kUiFontSize - 2, LIGHTGRAY);
    char cap_hint[148];
    std::snprintf(cap_hint, sizeof(cap_hint),
                  "Remote skaters delayed B ticks — host unchanged. Cap %u (~%.0f ms). Tab: port » B ticks » interp ms.",
                  static_cast<unsigned>(kMaxInputDelayTicks),
                  static_cast<double>(kMaxInputDelayTicks) * static_cast<double>(App::kFixedDt) * 1000.);
    DrawText(cap_hint, 48, 232, kUiFontSize - 10, Color{135, 140, 158, 255});

    Rectangle field_delay{48.f, 294.f, 120.f, 44.f};
    bool const hover_delay = point_in(field_delay, mp);
    Color delay_bg = delay_focus ? Color{40, 45, 70, 255} : Color{30, 35, 55, 255};
    if ((!delay_focus) && hover_delay) {
        delay_bg = ColorBrightness(delay_bg, 0.14f);
    }
    DrawRectangleRec(field_delay, delay_bg);
    Color delay_br{};
    float delay_thick = 2.f;
    if (delay_focus) {
        delay_br = SKYBLUE;
        delay_thick = 3.f;
    } else if (hover_delay) {
        delay_br = Color{120, 200, 255, 255};
        delay_thick = 2.5f;
    } else {
        delay_br = DARKGRAY;
    }
    DrawRectangleLinesEx(field_delay, delay_thick, delay_br);
    DrawText(options_delay_ticks_buf_.c_str(),
             static_cast<int>(field_delay.x + 12.f),
             static_cast<int>(field_delay.y + 10.f),
             kUiFontSize,
             RAYWHITE);

    DrawText("WAN client interp replay lag (ms, local only)", 48, 358, kUiFontSize - 2, LIGHTGRAY);
    char interp_hint[148];
    std::snprintf(
        interp_hint, sizeof(interp_hint),
        "Interpolation sample = GetTime() minus this lag. 0 ms = freshest snap (often jittery). Cap %u ms.",
        static_cast<unsigned>(kMaxClientInterpDelayMs));
    DrawText(interp_hint, 48, 380, kUiFontSize - 10, Color{135, 140, 158, 255});

    Rectangle field_interp_ms{48.f, 442.f, 132.f, 44.f};
    bool const hover_interp = point_in(field_interp_ms, mp);
    Color interp_bg = interp_focus ? Color{40, 45, 70, 255} : Color{30, 35, 55, 255};
    if ((!interp_focus) && hover_interp) {
        interp_bg = ColorBrightness(interp_bg, 0.14f);
    }
    DrawRectangleRec(field_interp_ms, interp_bg);
    Color interp_br{};
    float interp_thick = 2.f;
    if (interp_focus) {
        interp_br = SKYBLUE;
        interp_thick = 3.f;
    } else if (hover_interp) {
        interp_br = Color{120, 200, 255, 255};
        interp_thick = 2.5f;
    } else {
        interp_br = DARKGRAY;
    }
    DrawRectangleLinesEx(field_interp_ms, interp_thick, interp_br);
    DrawText(options_interp_ms_buf_.c_str(),
             static_cast<int>(field_interp_ms.x + 12.f),
             static_cast<int>(field_interp_ms.y + 10.f),
             kUiFontSize,
             RAYWHITE);

    Rectangle back{48.f, static_cast<float>(GetScreenHeight() - 140), 260.f, 52.f};
    bool const hover_back = point_in(back, mp);

    if (!feedback_line_.empty()) {
        DrawText(feedback_line_.c_str(),
                 48,
                 static_cast<int>(back.y - 36.f),
                 kUiFontSize - 2,
                 feedback_error_ ? Color{235, 100, 100, 255} : Color{230, 200, 120, 255});
    }

    DrawRectangleRec(back, hover_back ? ColorBrightness(MAROON, 0.22f) : MAROON);
    DrawRectangleLinesEx(back, hover_back ? 3.5f : 2.f, hover_back ? GOLD : RAYWHITE);
    DrawText("Save & Back", static_cast<int>(back.x + 44), static_cast<int>(back.y + 14.f),
             kUiFontSize, hover_back ? GOLD : WHITE);

    if (IsKeyPressed(KEY_TAB)) {
        if (udp_focus) {
            options_edit_field_ = OptionsEditField::RemoteInputDelayTicks;
        } else if (delay_focus) {
            options_edit_field_ = OptionsEditField::ClientInterpLagMs;
        } else {
            options_edit_field_ = OptionsEditField::UdpPort;
        }
    }

    if (udp_focus) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= static_cast<int>('0') && key <= static_cast<int>('9')) {
                if (options_port_buf_.size() < 5U) {
                    options_port_buf_.push_back(static_cast<char>(key));
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && (!options_port_buf_.empty())) {
            options_port_buf_.pop_back();
        }
    } else if (delay_focus) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= static_cast<int>('0') && key <= static_cast<int>('9')) {
                if (options_delay_ticks_buf_.size() < 4U) {
                    options_delay_ticks_buf_.push_back(static_cast<char>(key));
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && (!options_delay_ticks_buf_.empty())) {
            options_delay_ticks_buf_.pop_back();
        }
    } else if (interp_focus) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= static_cast<int>('0') && key <= static_cast<int>('9')) {
                if (options_interp_ms_buf_.size() < 4U) {
                    options_interp_ms_buf_.push_back(static_cast<char>(key));
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && (!options_interp_ms_buf_.empty())) {
            options_interp_ms_buf_.pop_back();
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (point_in(back, mp)) {
            std::uint16_t parsed_port = 0;
            std::uint8_t parsed_delay = 0;
            std::uint16_t parsed_interp_ms = 0;
            if (!parse_udp_port_string(options_port_buf_, parsed_port)) {
                feedback_error_ = true;
                feedback_line_ = "Invalid UDP port — use digits 1-65535.";
            } else if (!parse_delay_ticks_string(options_delay_ticks_buf_, parsed_delay)) {
                feedback_error_ = true;
                feedback_line_ = "Invalid delay — digits only, max matches cap.";
            } else if (!parse_interp_delay_ms_string(options_interp_ms_buf_, kMaxClientInterpDelayMs,
                                                      parsed_interp_ms)) {
                feedback_error_ = true;
                feedback_line_ = "Invalid interp lag — digits only, max 500 ms.";
            } else {
                game_port_ = parsed_port;
                join_port_buf_ = options_port_buf_;
                stored_remote_delay_ticks_ = parsed_delay;
                stored_client_interp_delay_ms_ = parsed_interp_ms;
                (void)persist_user_settings();
                feedback_line_.clear();
                feedback_error_ = false;
                options_edit_field_ = OptionsEditField::UdpPort;
                screen_ = Screen::Home;
            }
        } else if (point_in(field_port, mp)) {
            options_edit_field_ = OptionsEditField::UdpPort;
        } else if (point_in(field_delay, mp)) {
            options_edit_field_ = OptionsEditField::RemoteInputDelayTicks;
        } else if (point_in(field_interp_ms, mp)) {
            options_edit_field_ = OptionsEditField::ClientInterpLagMs;
        }
    }
}

// --- Playing (world/camera, interp peers, puck, HUD, debug overlays) ---
void AppContext::draw_playing(float /*interp_alpha*/, float const frame_dt) {
    ClientInterpFrame r{};
    if (remote_client_) {
        wan_.snap_hist.sample_interp_frame(GetTime(), stored_client_interp_delay_ms_, snap_new_,
                                              remote_client_, r);
    }
    float const t_v = remote_client_ ? r.mix_t : 1.f;

    bool const host_world =
        listen_server_ != nullptr && listen_server_->valid() && match_running_;

    float cam_focus_x = rink_center_x();
    float cam_focus_y = rink_mid_y();
    resolve_playing_camera_focus(cam_focus_x, cam_focus_y);
    if (remote_client_ &&
        r.newer_snap.match_phase == static_cast<std::uint8_t>(MatchPhase::FaceoffCountdown)) {
        cam_focus_x = r.older_snap.puck_x + (r.newer_snap.puck_x - r.older_snap.puck_x) * t_v;
        cam_focus_y = r.older_snap.puck_y + (r.newer_snap.puck_y - r.older_snap.puck_y) * t_v;
    }
    update_playing_camera(cam_focus_x, cam_focus_y, frame_dt);

    ClearBackground(Color{12, 16, 24, 255});
    BeginMode2D(playing_cam_);
    zh::ui::draw_arctic_arena_rink(rink_sprites_);

    float draw_px = 0.f;
    float draw_py = 0.f;
    std::uint32_t display_tick = 0;

    std::uint8_t view_slot_team_bmask = 0;
    std::uint8_t view_slot_goalie_mask = 0;
    bool view_host_on_team_b = false;
    bool view_host_is_goalie = false;
    if (host_world) {
        view_slot_team_bmask = lobby_slot_team_b_mask_;
        view_slot_goalie_mask = lobby_slot_goalie_mask_;
        view_host_on_team_b = lobby_host_team_ != 0;
        view_host_is_goalie = lobby_host_goalie_ != 0;
    } else if (remote_client_) {
        view_slot_team_bmask = r.newer_snap.match_slot_team_b_mask;
        view_slot_goalie_mask = r.newer_snap.match_slot_goalie_mask;
        view_host_on_team_b = r.newer_snap.match_host_team != 0;
        view_host_is_goalie = r.newer_snap.match_host_goalie != 0;
    }

    if (host_world) {
        draw_px = world_.puck_x;
        draw_py = world_.puck_y;
        display_tick = world_.server_tick;
    } else if (remote_client_) {
        display_tick = r.newer_snap.server_tick;
        if (wan_.skater.seeded()) {
            draw_px = wan_.puck.puck_x();
            draw_py = wan_.puck.puck_y();
        } else {
            draw_px = r.older_snap.puck_x + (r.newer_snap.puck_x - r.older_snap.puck_x) * t_v;
            draw_py = r.older_snap.puck_y + (r.newer_snap.puck_y - r.older_snap.puck_y) * t_v;
        }
    }

    float hpx = 0.f;
    float hpy = 0.f;
    float hmwx = 0.f;
    float hmwy = 0.f;
    bool draw_host_way = false;
    if (host_world) {
        hpx = world_.host_sk_x;
        hpy = world_.host_sk_y;
        hmwx = world_.host_way_x;
        hmwy = world_.host_way_y;
        draw_host_way = world_.host_has_waypoint;
    } else if (remote_client_) {
        hpx = r.older_snap.host_px + (r.newer_snap.host_px - r.older_snap.host_px) * t_v;
        hpy = r.older_snap.host_py + (r.newer_snap.host_py - r.older_snap.host_py) * t_v;

        draw_host_way = r.newer_snap.host_has_waypoint != 0;
        if (r.older_snap.host_has_waypoint != 0 && r.newer_snap.host_has_waypoint != 0) {
            hmwx =
                r.older_snap.host_way_x + (r.newer_snap.host_way_x - r.older_snap.host_way_x) * t_v;
            hmwy =
                r.older_snap.host_way_y + (r.newer_snap.host_way_y - r.older_snap.host_way_y) * t_v;
        } else if (r.newer_snap.host_has_waypoint != 0) {
            hmwx = r.newer_snap.host_way_x;
            hmwy = r.newer_snap.host_way_y;
        } else {
            draw_host_way = false;
        }
    }

    skater_fx_.begin_frame();

    if (host_world || remote_client_) {
        bool host_boost = false;
        bool host_slide = false;
        float hvx_fx = 0.f;
        float hvy_fx = 0.f;
        if (host_world) {
            hvx_fx = world_.host_sk_vx;
            hvy_fx = world_.host_sk_vy;
            host_boost = world_.host_boost_overspeed_ticks > 0U;
            host_slide = world_.host_slide_decel_ticks > 0U;
        } else {
            float hface_dummy_x = 1.f;
            float hface_dummy_y = 0.f;
            infer_draw_skater_axes(r.older_snap.host_px, r.older_snap.host_py, r.newer_snap.host_px,
                                   r.newer_snap.host_py, 1.f, 0.f, hvx_fx, hvy_fx, hface_dummy_x,
                                   hface_dummy_y);
            host_boost = false;
        }
        skater_fx_.feed(zh::ui::SkaterFxSlot::Host, hpx, hpy, hvx_fx, hvy_fx, host_boost, host_slide);
    }

    auto const feed_remote_slot_fx = [&](std::size_t const i, float const sx, float const sy,
                                         float const svx, float const svy, bool const boost_trail,
                                         bool const slide_spray) {
        auto const slot = static_cast<zh::ui::SkaterFxSlot>(static_cast<std::uint8_t>(i) + 1U);
        skater_fx_.feed(slot, sx, sy, svx, svy, boost_trail, slide_spray);
    };

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        float sx = 0.f;
        float sy = 0.f;
        float svx = 0.f;
        float svy = 0.f;
        bool boost_trail = false;
        bool slide_spray = false;

        if (host_world) {
            if (!slot_active_[i]) {
                continue;
            }
            sx = world_.slot_sk_x[i];
            sy = world_.slot_sk_y[i];
            svx = world_.slot_sk_vx[i];
            svy = world_.slot_sk_vy[i];
            boost_trail = world_.slot_boost_overspeed_ticks[i] > 0U;
            slide_spray = world_.slot_slide_decel_ticks[i] > 0U;
        } else if (remote_client_) {
            bool const occ =
                ((r.newer_snap.slot_occupied_mask >> static_cast<unsigned>(i)) & 1U) != 0U;
            if (!occ) {
                continue;
            }
            bool const is_my_here =
                wan_.my_slot < kRemoteSlots && static_cast<std::size_t>(wan_.my_slot) == i;
            if (wan_.skater.seeded() && is_my_here) {
                sx = wan_.skater.px();
                sy = wan_.skater.py();
                svx = wan_.skater.vx();
                svy = wan_.skater.vy();
                boost_trail = wan_.skater.boost_overspeed_ticks() > 0U;
                slide_spray = wan_.skater.slide_decel_ticks() > 0U;
            } else {
                sx = r.older_snap.skater_px[i] +
                     (r.newer_snap.skater_px[i] - r.older_snap.skater_px[i]) * t_v;
                sy = r.older_snap.skater_py[i] +
                     (r.newer_snap.skater_py[i] - r.older_snap.skater_py[i]) * t_v;
                float sface_dummy_x = 1.f;
                float sface_dummy_y = 0.f;
                infer_draw_skater_axes(r.older_snap.skater_px[i], r.older_snap.skater_py[i],
                                       r.newer_snap.skater_px[i], r.newer_snap.skater_py[i], 1.f,
                                       0.f, svx, svy, sface_dummy_x, sface_dummy_y);
                boost_trail = false;
            }
        } else {
            continue;
        }

        feed_remote_slot_fx(i, sx, sy, svx, svy, boost_trail, slide_spray);
    }

    skater_fx_.update(frame_dt);
    skater_fx_.draw();

    DrawCircleV({draw_px, draw_py}, kPuckDrawRadius, BLACK);
    DrawCircleLinesV({draw_px, draw_py}, kPuckDrawRadius, BLACK);

    Color const fb_team_a_fill = ColorFromHSV(200.f, 0.74f, 0.93f);
    Color const fb_team_b_fill = ColorFromHSV(6.f, 0.74f, 0.93f);

    auto draw_one_timer_bar = [](float sk_x, float sk_y, float remaining_frac) {
        constexpr float bar_w = 64.f;
        constexpr float bar_h = 7.f;
        float const x = sk_x - bar_w * 0.5f;
        float const y = sk_y - kSkaterDrawRadius - 36.f;
        DrawRectangle(x, y, bar_w, bar_h, Color{16, 24, 32, 210});
        remaining_frac = std::clamp(remaining_frac, 0.f, 1.f);
        if (remaining_frac > 0.f) {
            DrawRectangle(x, y, bar_w * remaining_frac, bar_h, Color{90, 220, 255, 255});
        }
        DrawRectangleLinesEx(Rectangle{x, y, bar_w, bar_h}, 1.5f, Color{180, 230, 255, 220});
    };

    auto draw_goalie_shield_ring = [](float sk_x, float sk_y, float remaining_frac) {
        remaining_frac = std::clamp(remaining_frac, 0.f, 1.f);
        float const pulse = 0.55f + 0.45f * remaining_frac;
        unsigned char const alpha =
            static_cast<unsigned char>(std::clamp(70.f + 150.f * pulse, 40.f, 230.f));
        float const shield_r = kGoalieShieldRadius;
        DrawCircleLinesV({sk_x, sk_y}, shield_r, Color{120, 210, 255, alpha});
        DrawRing({sk_x, sk_y}, shield_r - 6.f, shield_r - 2.f, 0.f, 360.f, 56,
                 Color{190, 245, 255, static_cast<unsigned char>(alpha * 0.75f)});
        DrawCircleV({sk_x, sk_y}, shield_r * 0.22f,
                    Color{160, 230, 255, static_cast<unsigned char>(alpha * 0.35f)});
    };

    if (host_world || remote_client_) {
        Color const fb_host = view_host_on_team_b ? fb_team_b_fill : fb_team_a_fill;
        zh::ui::draw_team_player_sprite(
            player_tex_team_a_, player_tex_team_b_, player_goalie_tex_team_a_,
            player_goalie_tex_team_b_, player_tex_ready_, hpx, hpy, view_host_on_team_b,
            view_host_is_goalie, player_draw_radius(view_host_is_goalie) + 1.5f,
            player_sprite_draw_px(view_host_is_goalie), WHITE, fb_host);
        if (host_world) {
            draw_player_stick_hitbox(hpx, hpy, world_.host_sk_vx, world_.host_sk_vy, world_.host_facing_x,
                                     world_.host_facing_y, true);
        } else {
            float hvx = 0.f;
            float hvy = 0.f;
            float hface_x = 1.f;
            float hface_y = 0.f;
            infer_draw_skater_axes(r.older_snap.host_px, r.older_snap.host_py, r.newer_snap.host_px,
                                   r.newer_snap.host_py, 1.f, 0.f, hvx, hvy, hface_x, hface_y);
            draw_player_stick_hitbox(hpx, hpy, hvx, hvy, hface_x, hface_y, false);
        }
        if (draw_host_way) {
            DrawCircleLines(static_cast<int>(hmwx), static_cast<int>(hmwy), 9.f,
                            Color{60, 255, 140, 255});
            DrawCircleLines(static_cast<int>(hmwx), static_cast<int>(hmwy), 12.f,
                            Color{140, 255, 170, 200});
        }
        float host_one_timer_frac = 0.f;
        if (host_world && world_.host_one_timer_ticks > 0U) {
            host_one_timer_frac = world_.host_one_timer_fraction();
        } else if (remote_client_ && r.newer_snap.host_one_timer_ticks > 0U) {
            host_one_timer_frac =
                one_timer_fraction(static_cast<std::uint32_t>(r.newer_snap.host_one_timer_ticks));
        }
        if (host_one_timer_frac > 0.f) {
            draw_one_timer_bar(hpx, hpy, host_one_timer_frac);
        }
        float host_shield_frac = 0.f;
        if (host_world && view_host_is_goalie && world_.host_shield_ticks > 0U) {
            host_shield_frac = world_.host_shield_fraction();
        } else if (remote_client_ && view_host_is_goalie && r.newer_snap.host_shield_ticks > 0U) {
            host_shield_frac = goalie_shield_fraction(
                static_cast<std::uint32_t>(r.newer_snap.host_shield_ticks));
        }
        if (host_shield_frac > 0.f) {
            draw_goalie_shield_ring(hpx, hpy, host_shield_frac);
        }
    }

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        std::uint8_t mask =
            host_world ? occupied_slots_mask_ : r.newer_snap.slot_occupied_mask;
        if (((mask >> static_cast<unsigned>(i)) & 1U) == 0U) {
            continue;
        }
        float sx = 0.f;
        float sy = 0.f;
        float wpx = 0.f;
        float wpy = 0.f;
        bool wp = false;
        float svx = 0.f;
        float svy = 0.f;
        float sface_x = 1.f;
        float sface_y = 0.f;

        if (host_world) {
            sx = world_.slot_sk_x[i];
            sy = world_.slot_sk_y[i];
            wpx = world_.slot_way_x[i];
            wpy = world_.slot_way_y[i];
            wp = world_.slot_has_waypoint[i];
            svx = world_.slot_sk_vx[i];
            svy = world_.slot_sk_vy[i];
            sface_x = world_.slot_facing_x[i];
            sface_y = world_.slot_facing_y[i];
        } else if (remote_client_) {
            bool const is_my_here =
                wan_.my_slot < kRemoteSlots && static_cast<std::size_t>(wan_.my_slot) == i;
            if (wan_.skater.seeded() && is_my_here) {
                sx = wan_.skater.px();
                sy = wan_.skater.py();
                wpx = wan_.skater.way_x();
                wpy = wan_.skater.way_y();
                wp = wan_.skater.has_waypoint();
                svx = wan_.skater.vx();
                svy = wan_.skater.vy();
                sface_x = wan_.skater.face_x();
                sface_y = wan_.skater.face_y();
            } else {
                sx = r.older_snap.skater_px[i] +
                     (r.newer_snap.skater_px[i] - r.older_snap.skater_px[i]) * t_v;
                sy = r.older_snap.skater_py[i] +
                     (r.newer_snap.skater_py[i] - r.older_snap.skater_py[i]) * t_v;
                infer_draw_skater_axes(r.older_snap.skater_px[i], r.older_snap.skater_py[i],
                                       r.newer_snap.skater_px[i], r.newer_snap.skater_py[i], 1.f,
                                       0.f, svx, svy, sface_x, sface_y);
                std::uint8_t const wm_prev = r.older_snap.skater_waypoint_mask;
                std::uint8_t const wm_new = r.newer_snap.skater_waypoint_mask;
                if (((wm_prev >> static_cast<unsigned>(i)) & 1U) != 0U &&
                    ((wm_new >> static_cast<unsigned>(i)) & 1U) != 0U) {
                    wpx = r.older_snap.skater_way_x[i] +
                          (r.newer_snap.skater_way_x[i] - r.older_snap.skater_way_x[i]) * t_v;
                    wpy = r.older_snap.skater_way_y[i] +
                          (r.newer_snap.skater_way_y[i] - r.older_snap.skater_way_y[i]) * t_v;
                    wp = true;
                } else if (((wm_new >> static_cast<unsigned>(i)) & 1U) != 0U) {
                    wpx = r.newer_snap.skater_way_x[i];
                    wpy = r.newer_snap.skater_way_y[i];
                    wp = true;
                }
            }
        }

        bool const roster_team_b =
            ((view_slot_team_bmask >> static_cast<unsigned>(i)) & 1U) != 0U;
        bool const roster_goalie =
            ((view_slot_goalie_mask >> static_cast<unsigned>(i)) & 1U) != 0U;
        bool const is_my_slot =
            remote_client_ && wan_.my_slot < kRemoteSlots &&
            static_cast<std::size_t>(wan_.my_slot) == i;
        Color const rim_col = is_my_slot ? GOLD : ColorBrightness(WHITE, -0.12f);
        Color const fb_here = roster_team_b ? fb_team_b_fill : fb_team_a_fill;
        zh::ui::draw_team_player_sprite(
            player_tex_team_a_, player_tex_team_b_, player_goalie_tex_team_a_,
            player_goalie_tex_team_b_, player_tex_ready_, sx, sy, roster_team_b, roster_goalie,
            player_draw_radius(roster_goalie) + 0.6f, player_sprite_draw_px(roster_goalie), rim_col,
            fb_here);
        draw_player_stick_hitbox(sx, sy, svx, svy, sface_x, sface_y, is_my_slot);

        if (wp) {
            DrawCircleLines(static_cast<int>(wpx), static_cast<int>(wpy), 7.f,
                            Color{230, 200, 60, 255});
            DrawCircleLines(static_cast<int>(wpx), static_cast<int>(wpy), 10.f,
                            Color{255, 220, 100, 200});
        }

        float one_timer_frac = 0.f;
        if (host_world) {
            if (world_.slot_one_timer_ticks[i] > 0U) {
                one_timer_frac = one_timer_fraction(world_.slot_one_timer_ticks[i]);
            }
        } else if (remote_client_) {
            if (is_my_slot && wan_.skater.seeded()) {
                one_timer_frac = wan_.skater.one_timer_fraction();
            } else if (r.newer_snap.slot_one_timer_ticks[i] > 0U) {
                one_timer_frac =
                    one_timer_fraction(static_cast<std::uint32_t>(r.newer_snap.slot_one_timer_ticks[i]));
            }
        }
        if (one_timer_frac > 0.f) {
            draw_one_timer_bar(sx, sy, one_timer_frac);
        }

        float shield_frac = 0.f;
        if (host_world) {
            if (roster_goalie && world_.slot_shield_ticks[i] > 0U) {
                shield_frac = goalie_shield_fraction(world_.slot_shield_ticks[i]);
            }
        } else if (remote_client_) {
            if (is_my_slot && wan_.skater.seeded() && wan_.skater.is_goalie()) {
                shield_frac = wan_.skater.shield_fraction();
            } else if (roster_goalie && r.newer_snap.slot_shield_ticks[i] > 0U) {
                shield_frac = goalie_shield_fraction(
                    static_cast<std::uint32_t>(r.newer_snap.slot_shield_ticks[i]));
            }
        }
        if (shield_frac > 0.f) {
            draw_goalie_shield_ring(sx, sy, shield_frac);
        }
    }

    auto draw_shoot_power_bar = [](float sk_x, float sk_y, float charge_frac) {
        constexpr float bar_w = 72.f;
        constexpr float bar_h = 9.f;
        float const x = sk_x - bar_w * 0.5f;
        float const y = sk_y - kSkaterDrawRadius - 24.f;
        DrawRectangle(x, y, bar_w, bar_h, Color{24, 24, 28, 210});
        charge_frac = std::clamp(charge_frac, 0.f, 1.f);
        if (charge_frac > 0.f) {
            DrawRectangle(x, y, bar_w * charge_frac, bar_h, Color{255, 170, 50, 255});
        }
        DrawRectangleLinesEx(Rectangle{x, y, bar_w, bar_h}, 1.5f, RAYWHITE);
    };

    if (host_world && world_.puck_carrier == kPuckCarrierHost &&
        (world_.host_shoot_charge_ticks > 0U || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        draw_shoot_power_bar(world_.host_sk_x, world_.host_sk_y, world_.host_shoot_charge_fraction());
    } else if (remote_client_ && wan_.my_slot < kRemoteSlots && wan_.skater.seeded() &&
               wan_.puck.carrier() == static_cast<std::int8_t>(wan_.my_slot) &&
               (wan_.puck.shoot_charging() || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        draw_shoot_power_bar(wan_.skater.px(), wan_.skater.py(), wan_.puck.shoot_charge_fraction());
    }

    EndMode2D();

    int const fs_ctrl = kUiFontSize - 5;
    int const fs_net = kUiFontSize - 10;
    int hud_y = static_cast<int>(kPlayingHudInsetY + 8.f);
    int const hud_x = kPlayingHudLeftX;
    constexpr int kHudLineGap = 3;

    auto hud_advance = [&](int font_px) noexcept {
        hud_y += font_px + kHudLineGap;
    };

    char hud_controls[160];
    char hud_net[288];
    char hud_debug[220];
    if (remote_client_) {
        (void)std::snprintf(hud_controls, sizeof(hud_controls),
                            "Controls — R waypoint · Z boost · X slide · S stop · C one-timer · L steal · hold L shoot");
        std::uint32_t ack = 0;
        if (wan_.my_slot < kRemoteSlots) {
            ack = snap_new_.ack_seq[wan_.my_slot];
        }
        std::snprintf(
            hud_net, sizeof(hud_net),
            "Status — slot %u · tick %u · ack %u · WAN B=%u",
            static_cast<unsigned>(wan_.my_slot), static_cast<unsigned>(display_tick),
            static_cast<unsigned>(ack),
            static_cast<unsigned>(snap_new_.input_delay_ticks_b));
        std::snprintf(hud_debug, sizeof(hud_debug),
                      "Debug — replay lag %ums · snap buf %zu / %zu · sent inputs %zu / %zu",
                      static_cast<unsigned>(stored_client_interp_delay_ms_),
                      wan_.snap_hist.count(), zh::client::ClientSnapRingBuffer::capacity(),
                      wan_.sent_input.count(), zh::client::ClientSentInputRing::capacity());
    } else if (host_world) {
        (void)std::snprintf(hud_controls, sizeof(hud_controls),
                            "Controls — R waypoint · Z boost · X slide · S stop · C one-timer · L steal · hold L shoot");
        std::snprintf(hud_net, sizeof(hud_net),
                      "Status — tick %u · UDP %u · WAN delay B=%u (remotes)",
                      static_cast<unsigned>(display_tick), static_cast<unsigned>(game_port_),
                      static_cast<unsigned>(world_.input_delay_ticks_b));
        hud_debug[0] = '\0';
    } else {
        hud_controls[0] = '\0';
        hud_net[0] = '\0';
        hud_debug[0] = '\0';
    }

    if (hud_controls[0] != '\0') {
        DrawText(hud_controls, hud_x, hud_y, fs_ctrl, WHITE);
        hud_advance(fs_ctrl);
    }
    if (hud_net[0] != '\0') {
        DrawText(hud_net, hud_x, hud_y, fs_net, kPlayingHudNetCol);
        hud_advance(fs_net);
    }
    if (hud_debug[0] != '\0') {
        DrawText(hud_debug, hud_x, hud_y, fs_net, kPlayingHudDebugCol);
        hud_advance(fs_net);
    }

    if (host_world && host_session_hint_[0] != '\0' &&
        GetTime() < host_session_hint_until_sec_) {
        DrawText(host_session_hint_, hud_x, hud_y, kUiFontSize - 8,
                 Color{240, 220, 150, 255});
        hud_advance(kUiFontSize - 8);
    }

    int hud_y_stack = hud_y;

    int const kBoostHudFs = kUiFontSize - 10;
    if (remote_client_ || host_world) {
        char boost_ln[152]{};
        auto const write_boost_hud = [](char *out,
                                        std::size_t out_sz,
                                        bool const is_goalie,
                                        std::uint32_t const cd0,
                                        std::uint32_t const cd1) {
            unsigned ready = (cd0 == 0U) ? 1U : 0U;
            if (is_goalie && cd1 == 0U) {
                ++ready;
            }
            unsigned const max_charges = is_goalie ? kGoalieBoostCharges : 1U;
            if (ready == max_charges) {
                if (max_charges > 1U) {
                    std::snprintf(out, out_sz, "Boost (Z): %u ready", ready);
                } else {
                    (void)std::snprintf(out, out_sz, "Boost (Z): ready");
                }
            } else if (ready > 0U) {
                std::snprintf(out, out_sz, "Boost (Z): %u ready (%u recharging)", ready,
                              max_charges - ready);
            } else {
                std::uint32_t longest_cd = cd0;
                if (is_goalie && cd1 > longest_cd) {
                    longest_cd = cd1;
                }
                std::snprintf(out, out_sz, "Boost (Z): cooldown %.1fs",
                              static_cast<double>(longest_cd) * static_cast<double>(App::kFixedDt));
            }
        };

        if (host_world) {
            bool const host_goalie = (world_.match_host_goalie & 1U) != 0U;
            write_boost_hud(boost_ln, sizeof(boost_ln), host_goalie, world_.host_boost_cd_ticks[0],
                            world_.host_boost_cd_ticks[1]);
        } else if (remote_client_) {
            if (wan_.my_slot < kRemoteSlots) {
                unsigned const slot_ix = static_cast<unsigned>(wan_.my_slot);
                bool const my_goalie =
                    ((snap_new_.match_slot_goalie_mask >> slot_ix) & 1U) != 0U;
                write_boost_hud(boost_ln, sizeof(boost_ln), my_goalie,
                                static_cast<std::uint32_t>(
                                    snap_new_.slot_boost_cd_ticks[slot_ix]),
                                static_cast<std::uint32_t>(
                                    snap_new_.slot_boost_cd2_ticks[slot_ix]));
            } else {
                boost_ln[0] = '\0';
            }
        }
        if (boost_ln[0] != '\0') {
            DrawText(boost_ln, hud_x, hud_y_stack, kBoostHudFs, Color{200, 220, 120, 255});
            hud_y_stack += kBoostHudFs + 6;
        }
    }

    char puck_line[176];
    if (remote_client_ || host_world) {
        std::int8_t const carrier_view =
            host_world ? world_.puck_carrier : snap_new_.puck_carrier;
        format_puck_carrier_line(puck_line, sizeof(puck_line), carrier_view);
        DrawText(puck_line, hud_x, hud_y_stack, kUiFontSize - 8,
                 Color{190, 210, 228, 255});
    }

    char score_line[128];
    if (remote_client_ || host_world) {
        unsigned const gw =
            host_world ? static_cast<unsigned>(world_.goals_left_side)
                       : static_cast<unsigned>(r.newer_snap.goals_left_side);
        unsigned const ge =
            host_world ? static_cast<unsigned>(world_.goals_right_side)
                       : static_cast<unsigned>(r.newer_snap.goals_right_side);
        std::snprintf(score_line, sizeof(score_line), "Goals  west rim %u  ·  east rim %u", gw,
                      ge);
        int const fs_sc = kUiFontSize - 2;
        int const tw_sc = MeasureText(score_line, fs_sc);
        int const top_bar_y = 8;
        float const hud_w = static_cast<float>(GetScreenWidth());
        DrawText(score_line,
                 static_cast<int>(hud_w - static_cast<float>(tw_sc) - 34.f),
                 top_bar_y, fs_sc, Color{255, 226, 150, 255});

        std::uint8_t match_phase = 0;
        std::uint8_t match_period = 1;
        std::uint16_t period_ticks_left = 0;
        std::uint8_t faceoff_ticks = 0;
        if (host_world) {
            match_phase = static_cast<std::uint8_t>(world_.phase);
            match_period = world_.period;
            period_ticks_left = static_cast<std::uint16_t>(
                std::min(world_.period_ticks_remaining, static_cast<std::uint32_t>(65535U)));
            faceoff_ticks = static_cast<std::uint8_t>(
                std::min(world_.faceoff_countdown_ticks, static_cast<std::uint32_t>(255U)));
        } else if (remote_client_) {
            match_phase = r.newer_snap.match_phase;
            match_period = r.newer_snap.match_period;
            period_ticks_left = r.newer_snap.period_ticks_remaining;
            faceoff_ticks = r.newer_snap.faceoff_countdown_ticks;
        }

        char timer_line[24];
        int const fs_tm = kUiFontSize + 8;
        if (match_phase == static_cast<std::uint8_t>(MatchPhase::MatchEnded)) {
            (void)std::snprintf(timer_line, sizeof(timer_line), "FINAL");
        } else if (match_phase == static_cast<std::uint8_t>(MatchPhase::Playing)) {
            int const period_secs = static_cast<int>(period_ticks_left / 60U);
            int const mm_show = period_secs / 60;
            int const ss_show = period_secs % 60;
            std::snprintf(timer_line, sizeof(timer_line), "P%u %02d:%02d",
                          static_cast<unsigned>(match_period), mm_show, ss_show);
        } else {
            int const countdown_secs =
                faceoff_countdown_secs_from_ticks(static_cast<std::uint32_t>(faceoff_ticks));
            std::snprintf(timer_line, sizeof(timer_line), "Faceoff %d",
                          std::max(1, countdown_secs));
        }
        int const tw_tm = MeasureText(timer_line, fs_tm);
        DrawText(timer_line,
                 static_cast<int>((hud_w - static_cast<float>(tw_tm)) * 0.5f),
                 top_bar_y, fs_tm, Color{220, 240, 255, 255});

        if (match_phase == static_cast<std::uint8_t>(MatchPhase::FaceoffCountdown) &&
            faceoff_ticks > 0U) {
            int const countdown_secs =
                faceoff_countdown_secs_from_ticks(static_cast<std::uint32_t>(faceoff_ticks));
            draw_faceoff_countdown_overlay(countdown_secs);
        }
    }

    Rectangle back = playing_leave_bounds();
    Vector2 const mp = GetMousePosition();
    bool const hover_leave = point_in(back, mp);
    DrawRectangleRec(back, hover_leave ? ColorBrightness(MAROON, 0.22f) : MAROON);
    DrawRectangleLinesEx(back, hover_leave ? 3.5f : 2.f, hover_leave ? GOLD : RAYWHITE);
    DrawText("Leave match", static_cast<int>(back.x + 38), static_cast<int>(back.y + 14.f),
             kUiFontSize,
             hover_leave ? GOLD : WHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (point_in(back, mp)) {
            stop_client();
            stop_listen_server();
            client_reset_session();
            host_reset_session();
            screen_ = Screen::Home;
        }
    }
}

}  // namespace zh::detail
