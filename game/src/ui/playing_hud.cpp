#include "zh/ui/playing_hud.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/match_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zh::ui {

namespace {

using namespace zh::game;

constexpr int kHudTitleFs = 11;
constexpr int kHudScoreFs = 44;
constexpr int kHudTimerFs = 34;
constexpr int kHudLabelFs = 13;
constexpr int kAbilityFs = 14;
constexpr int kKeyFs = 12;

Color const kPanelBg{16, 20, 30, 235};
Color const kPanelEdge{58, 72, 98, 255};
Color const kPanelGlow{90, 130, 190, 80};
Color const kRedTeam{220, 58, 58, 255};
Color const kBlueTeam{58, 130, 230, 255};
Color const kReadyGreen{62, 196, 98, 255};
Color const kMuted{140, 152, 172, 255};

constexpr float kPlayingAbilitySlotSize = 62.f;
constexpr float kPlayingAbilitySlotGap = 10.f;
constexpr float kPlayingAbilityBarBottomPad = 22.f;
constexpr float kPlayingShootBarHeight = 18.f;
constexpr float kPlayingShootBarGap = 12.f;

struct PlayingAbilityBarLayout {
    float x0{};
    float y0{};
    float total_w{};
    float slot{};
};

[[nodiscard]] Rectangle playing_hud_menu_bounds() noexcept {
    float const sw = static_cast<float>(GetScreenWidth());
    float const sh = static_cast<float>(GetScreenHeight());
    return Rectangle{sw - 132.f, sh - 58.f, 116.f, 42.f};
}

[[nodiscard]] Rectangle playing_hud_settings_bounds() noexcept {
    float const sh = static_cast<float>(GetScreenHeight());
    return Rectangle{14.f, sh - 58.f, 42.f, 42.f};
}

[[nodiscard]] PlayingAbilityBarLayout playing_ability_bar_layout() noexcept {
    float const sw = static_cast<float>(GetScreenWidth());
    float const sh = static_cast<float>(GetScreenHeight());
    float const slot = kPlayingAbilitySlotSize;
    float const gap = kPlayingAbilitySlotGap;
    float const total_w = slot * 5.f + gap * 4.f;
    float const x0 = (sw - total_w) * 0.5f;
    float const y0 = sh - slot - kPlayingAbilityBarBottomPad;
    return PlayingAbilityBarLayout{x0, y0, total_w, slot};
}

[[nodiscard]] Rectangle playing_shoot_bar_bounds(PlayingAbilityBarLayout const &layout) noexcept {
    float const bar_h = kPlayingShootBarHeight;
    float const y = layout.y0 - bar_h - kPlayingShootBarGap;
    return Rectangle{layout.x0, y, layout.total_w, bar_h};
}

void draw_panel(Rectangle r, Color accent, float border = 2.f) noexcept {
    DrawRectangleRec(r, kPanelBg);
    DrawRectangleLinesEx(r, border, kPanelEdge);
    Rectangle accent_bar{r.x + 2.f, r.y + 2.f, r.width - 4.f, 3.f};
    DrawRectangleRec(accent_bar, accent);
    DrawRectangle(static_cast<int>(r.x - 2), static_cast<int>(r.y - 2), static_cast<int>(r.width + 4),
                  static_cast<int>(r.height + 4), kPanelGlow);
}

void draw_centered_text(char const *text, Rectangle r, int font_size, Color color,
                        float y_offset = 0.f) noexcept {
    int const tw = MeasureText(text, font_size);
    int const tx = static_cast<int>(r.x + (r.width - static_cast<float>(tw)) * 0.5f);
    int const ty = static_cast<int>(r.y + (r.height - static_cast<float>(font_size)) * 0.5f +
                                    y_offset);
    DrawText(text, tx, ty, font_size, color);
}

void draw_text_with_outline(int x, int y, int font_size, char const *text, Color fill,
                            Color outline) noexcept {
    static int const offsets[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (auto const &off : offsets) {
        DrawText(text, x + off[0], y + off[1], font_size, outline);
    }
    DrawText(text, x, y, font_size, fill);
}

void draw_icon_lightning(Rectangle r, Color color) noexcept {
    float const cx = r.x + r.width * 0.5f;
    float const top = r.y + r.height * 0.12f;
    float const bot = r.y + r.height * 0.88f;
    float const mid = (top + bot) * 0.5f;
    Vector2 const pts[7] = {
        {cx + 1.f, top},
        {cx - 8.f, mid + 2.f},
        {cx - 1.f, mid + 2.f},
        {cx - 4.f, bot},
        {cx + 9.f, mid - 4.f},
        {cx + 1.f, mid - 4.f},
        {cx + 5.f, top},
    };
    for (int i = 0; i < 6; ++i) {
        DrawLineEx(pts[i], pts[i + 1], 2.5f, color);
    }
}

void draw_icon_shield(Rectangle r, Color color) noexcept {
    float const cx = r.x + r.width * 0.5f;
    float const top = r.y + 10.f;
    float const bot = r.y + r.height - 8.f;
    Vector2 const pts[5] = {
        {cx, top},
        {r.x + r.width - 12.f, top + 12.f},
        {cx, bot},
        {r.x + 12.f, top + 12.f},
        {cx, top},
    };
    for (int i = 0; i < 4; ++i) {
        DrawLineEx(pts[i], pts[i + 1], 2.5f, color);
    }
}

void draw_icon_target(Rectangle r, Color color) noexcept {
    float const cx = r.x + r.width * 0.5f;
    float const cy = r.y + r.height * 0.5f;
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 16, color);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 8, color);
    DrawLineEx({cx - 12.f, cy}, {cx + 12.f, cy}, 2.f, color);
    DrawLineEx({cx, cy - 12.f}, {cx, cy + 12.f}, 2.f, color);
}

void draw_icon_stop(Rectangle r, Color color) noexcept {
    float const cx = r.x + r.width * 0.5f;
    float const cy = r.y + r.height * 0.5f;
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 17, color);
    DrawLineEx({cx - 10.f, cy - 10.f}, {cx + 10.f, cy + 10.f}, 3.f, color);
    DrawLineEx({cx + 10.f, cy - 10.f}, {cx - 10.f, cy + 10.f}, 3.f, color);
}

void draw_icon_timer(Rectangle r, Color color) noexcept {
    float const cx = r.x + r.width * 0.5f;
    float const cy = r.y + r.height * 0.5f;
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 16, color);
    DrawLineEx({cx, cy}, {cx + 10.f, cy - 8.f}, 2.5f, color);
    DrawLineEx({cx, cy}, {cx, cy + 11.f}, 2.f, color);
}

void draw_ability_slot(Rectangle r,
                       char key,
                       bool active,
                       bool selected,
                       void (*icon_fn)(Rectangle, Color),
                       int charges = 0) noexcept {
    Color const border = selected ? Color{90, 190, 255, 255}
                                  : (active ? Color{120, 140, 170, 255} : kPanelEdge);
    Color const fill = selected ? Color{24, 34, 52, 255} : Color{20, 26, 38, 255};
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, selected ? 3.f : 2.f, border);
    if (selected) {
        DrawRectangleLinesEx(
            Rectangle{r.x - 3.f, r.y - 3.f, r.width + 6.f, r.height + 6.f}, 2.f,
            Color{90, 190, 255, 90});
    }

    char key_lbl[2] = {key, '\0'};
    DrawText(key_lbl, static_cast<int>(r.x + 6.f), static_cast<int>(r.y + 4.f), kKeyFs, OFF_WHITE);

    Rectangle const icon_area{r.x + 10.f, r.y + 16.f, r.width - 20.f, r.height - 28.f};
    Color const icon_col = active ? OFF_WHITE : kMuted;
    icon_fn(icon_area, icon_col);

    if (charges > 0) {
        char charge_txt[16];
        std::snprintf(charge_txt, sizeof(charge_txt), "%d", charges);
        int const cw = MeasureText(charge_txt, kKeyFs);
        DrawText(charge_txt, static_cast<int>(r.x + r.width - static_cast<float>(cw) - 6.f),
                 static_cast<int>(r.y + r.height - 18.f), kKeyFs, kReadyGreen);
        float const bar_w = r.width - 16.f;
        float const bar_x = r.x + 8.f;
        float const bar_y = r.y + r.height - 8.f;
        for (unsigned i = 0; i < std::min(static_cast<unsigned>(charges), 2U); ++i) {
            DrawRectangle(static_cast<int>(bar_x + static_cast<float>(i) * (bar_w * 0.52f + 4.f)),
                          static_cast<int>(bar_y), static_cast<int>(bar_w * 0.48f), 3,
                          kReadyGreen);
        }
    }
}

void draw_top_scoreboard(PlayingHudModel const &model) noexcept {
    float const sw = static_cast<float>(GetScreenWidth());
    float const cx = sw * 0.5f;
    float const top_y = 10.f;
    float const bar_h = 74.f;
    float const center_w = 320.f;

    Rectangle const center_panel{cx - center_w * 0.5f, top_y, center_w, bar_h};

    draw_panel(center_panel, Color{120, 140, 180, 255}, 2.5f);

    char score_w[8];
    char score_e[8];
    std::snprintf(score_w, sizeof(score_w), "%u", model.goals_west);
    std::snprintf(score_e, sizeof(score_e), "%u", model.goals_east);

    Rectangle score_w_r{center_panel.x + 18.f, center_panel.y + 8.f, 72.f, center_panel.height - 16.f};
    Rectangle score_e_r{center_panel.x + center_panel.width - 90.f, center_panel.y + 8.f, 72.f,
                        center_panel.height - 16.f};
    draw_centered_text(score_w, score_w_r, kHudScoreFs, kRedTeam, 4.f);
    draw_centered_text(score_e, score_e_r, kHudScoreFs, kBlueTeam, 4.f);

    char timer_txt[24];
    if (model.match_phase == static_cast<std::uint8_t>(MatchPhase::MatchEnded)) {
        (void)std::snprintf(timer_txt, sizeof(timer_txt), "FINAL");
    } else if (model.match_phase == static_cast<std::uint8_t>(MatchPhase::Playing)) {
        int const period_secs = static_cast<int>(model.period_ticks_remaining / 60U);
        int const mm = period_secs / 60;
        int const ss = period_secs % 60;
        std::snprintf(timer_txt, sizeof(timer_txt), "%d:%02d", mm, ss);
    } else {
        int const secs = faceoff_countdown_secs_from_ticks(model.faceoff_countdown_ticks);
        std::snprintf(timer_txt, sizeof(timer_txt), "%d", std::max(1, secs));
    }

    Rectangle timer_r{center_panel.x + 96.f, center_panel.y + 6.f, center_panel.width - 192.f,
                      center_panel.height - 12.f};
    DrawCircle(static_cast<int>(timer_r.x + timer_r.width * 0.5f),
               static_cast<int>(timer_r.y + 8.f), 4, Color{230, 200, 80, 255});
    draw_centered_text(timer_txt, timer_r, kHudTimerFs, OFF_WHITE, 6.f);

    char period_lbl[12];
    std::snprintf(period_lbl, sizeof(period_lbl), "P%u", static_cast<unsigned>(model.match_period));
    DrawText(period_lbl, static_cast<int>(center_panel.x + center_panel.width * 0.5f - 10.f),
             static_cast<int>(center_panel.y + center_panel.height - 16.f), kHudTitleFs, kMuted);
}

void draw_shoot_charge_bar(float charge_frac, PlayingAbilityBarLayout const &layout) noexcept {
    Rectangle const bar = playing_shoot_bar_bounds(layout);

    DrawRectangle(static_cast<int>(bar.x), static_cast<int>(bar.y), static_cast<int>(bar.width),
                  static_cast<int>(bar.height), Color{24, 24, 28, 220});
    charge_frac = std::clamp(charge_frac, 0.f, 1.f);
    if (charge_frac > 0.f) {
        DrawRectangle(static_cast<int>(bar.x), static_cast<int>(bar.y),
                      static_cast<int>(bar.width * charge_frac), static_cast<int>(bar.height),
                      Color{255, 170, 50, 255});
    }
    DrawRectangleLinesEx(bar, 2.f, OFF_WHITE);
}

void draw_ability_bar(PlayingHudModel const &model) noexcept {
    PlayingAbilityBarLayout const layout = playing_ability_bar_layout();
    float const slot = layout.slot;
    float const gap = kPlayingAbilitySlotGap;
    float const x0 = layout.x0;
    float const y0 = layout.y0;

    unsigned const boost_charges =
        model.local_active ? static_cast<unsigned>(model.boost_ready) : 0U;
    bool const s_selected = model.s_down || model.lmb_shoot;

    draw_ability_slot(Rectangle{x0 + 0 * (slot + gap), y0, slot, slot}, 'Z', model.z_down, false,
                      draw_icon_lightning, static_cast<int>(boost_charges));
    draw_ability_slot(Rectangle{x0 + 1 * (slot + gap), y0, slot, slot}, 'X', model.x_down,
                      model.local_goalie && model.shield_active_frac > 0.f, draw_icon_shield);
    draw_ability_slot(Rectangle{x0 + 2 * (slot + gap), y0, slot, slot}, 'C', model.c_down,
                      model.one_timer_active, draw_icon_timer);
    draw_ability_slot(Rectangle{x0 + 3 * (slot + gap), y0, slot, slot}, 'R', model.r_down, false,
                      draw_icon_target);
    draw_ability_slot(Rectangle{x0 + 4 * (slot + gap), y0, slot, slot}, 'S', model.s_down,
                      s_selected, draw_icon_stop);
}

void draw_gear_icon(Rectangle r, Color color) noexcept {
    float const cx = r.x + r.width * 0.5f;
    float const cy = r.y + r.height * 0.5f;
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 12, color);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 5, color);
    for (int i = 0; i < 8; ++i) {
        float const ang = static_cast<float>(i) * 0.78539816339f;
        DrawLineEx({cx + std::cos(ang) * 8.f, cy + std::sin(ang) * 8.f},
                   {cx + std::cos(ang) * 14.f, cy + std::sin(ang) * 14.f}, 3.f, color);
    }
}

}  // namespace

PlayingHudLayout draw_playing_hud(PlayingHudModel const &model) noexcept {
    PlayingHudLayout layout{};
    layout.menu_button = playing_hud_menu_bounds();
    layout.settings_button = playing_hud_settings_bounds();

    if (model.show_match) {
        draw_top_scoreboard(model);
    }
    PlayingAbilityBarLayout const ability_layout = playing_ability_bar_layout();
    if (model.show_shoot_bar) {
        draw_shoot_charge_bar(model.shoot_charge, ability_layout);
    }
    draw_ability_bar(model);

    Vector2 const mp = GetMousePosition();
    bool const hover_menu = CheckCollisionPointRec(mp, layout.menu_button);
    bool const hover_settings = CheckCollisionPointRec(mp, layout.settings_button);

    DrawRectangleRec(layout.settings_button,
                     hover_settings ? Color{28, 36, 52, 240} : Color{20, 26, 38, 220});
    DrawRectangleLinesEx(layout.settings_button, 2.f,
                         hover_settings ? Color{120, 170, 230, 255} : kPanelEdge);
    draw_gear_icon(layout.settings_button, hover_settings ? OFF_WHITE : kMuted);

    Color const menu_fill = hover_menu ? Color{28, 40, 62, 245} : Color{18, 24, 36, 235};
    DrawRectangleRec(layout.menu_button, menu_fill);
    DrawRectangleLinesEx(layout.menu_button, 2.f,
                         hover_menu ? Color{90, 170, 255, 255} : kPanelEdge);
    draw_centered_text("Menu", layout.menu_button, kAbilityFs, hover_menu ? OFF_WHITE : kMuted);

    return layout;
}

void draw_player_name_tag(Vector2 screen, char const *name, Color team_color) noexcept {
    if (screen.x < -200.f || screen.y < -200.f) {
        return;
    }
    int const fs = 16;
    int const tw = MeasureText(name, fs);
    int const tx = static_cast<int>(screen.x - static_cast<float>(tw) * 0.5f);
    int const ty = static_cast<int>(screen.y - 34.f);
    draw_text_with_outline(tx, ty, fs, name, team_color, BLACK);
}

}  // namespace zh::ui
