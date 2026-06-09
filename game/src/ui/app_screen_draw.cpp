// Menu and lobby screen drawing (playing screen lives in app_screen_draw_playing.cpp).

#include "detail/app_context.hpp"
#include "zh/app.hpp"

#include <cstdint>
#include <cstdio>

#include "zh/gfx/gfx_compat.hpp"
#include "zh/game/lobby_caps.hpp"
#include "zh/ui/draw_widgets.hpp"
#include "zh/ui/form_parse.hpp"
#include "zh/ui/hit_test.hpp"

namespace zh::detail {

namespace {

using zh::ui::parse_delay_ticks_string;
using zh::ui::parse_interp_delay_ms_string;
using zh::ui::parse_udp_port_string;
using zh::ui::point_in;

constexpr int kUiFontSize = 24;

using namespace zh::game;

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
                     static_cast<int>(rect.y + 10.f), px, OFF_WHITE);
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
    DrawRectangleLinesEx(back, hover_back ? 3.5f : 2.f, hover_back ? GOLD : OFF_WHITE);
    DrawRectangleLinesEx(conn, hover_conn ? 3.5f : 2.f, hover_conn ? GOLD : OFF_WHITE);

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
                         hover_start ? GOLD : OFF_WHITE);
    DrawText("Start match", static_cast<int>(start.x + 44.f),
             static_cast<int>(start.y + 14.f), kUiFontSize,
             hover_start ? GOLD : WHITE);

    Rectangle back{380.f, static_cast<float>(GetScreenHeight() - 138), 280.f, 52.f};
    bool const hover_stop = CheckCollisionPointRec(mp, back);
    DrawRectangleRec(back, hover_stop ? ColorBrightness(MAROON, 0.22f) : MAROON);
    DrawRectangleLinesEx(back, hover_stop ? 4.f : 3.f, hover_stop ? GOLD : OFF_WHITE);
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
                         hover_cancel ? GOLD : OFF_WHITE);
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
             OFF_WHITE);

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
             OFF_WHITE);

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
             OFF_WHITE);

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
    DrawRectangleLinesEx(back, hover_back ? 3.5f : 2.f, hover_back ? GOLD : OFF_WHITE);
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

void AppContext::draw_render_test(float const frame_dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        screen_ = Screen::Home;
        return;
    }

    render_test_.update(frame_dt);
    DrawPlayingSkyGradient();
    render_test_.draw_world();
    render_test_.draw_hud();
    DrawFPS(GetScreenWidth() - 118, GetScreenHeight() - 28);
}

}  // namespace zh::detail
