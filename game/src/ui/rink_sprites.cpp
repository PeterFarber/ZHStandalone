// Modular rink art: walls, ice, goals, face-off circles from resources/rink/*.png.

#include "zh/ui/rink_sprites.hpp"

#include "detail/resource_paths.hpp"
#include "zh/game/map_definition.hpp"
#include "zh/game/map_geometry.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/rink_layout.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace zh::ui {

namespace {

using namespace zh::game;

// 9-slice margins from resources/rink/walls.png (1254×1254).
inline constexpr float kWallSliceL = 56.f;
inline constexpr float kWallSliceT = 60.f;
inline constexpr float kWallSliceR = 57.f;
inline constexpr float kWallSliceB = 47.f;

// Goal frame/net slices from resources/rink/goal.png.
inline constexpr float kGoalContentL = 375.f;
inline constexpr float kGoalContentT = 66.f;
inline constexpr float kGoalContentR = 878.f;
inline constexpr float kGoalContentB = 1186.f;
inline constexpr float kGoalSliceL = 55.f;
inline constexpr float kGoalSliceR = 55.f;

// Face-off circle art from resources/rink/circle.png.
inline constexpr float kCircleContentL = 162.f;
inline constexpr float kCircleContentT = 146.f;
inline constexpr float kCircleContentR = 1090.f;
inline constexpr float kCircleContentB = 1086.f;

[[nodiscard]] Texture2D try_load(char const *rel) noexcept {
    char const *path = zh::detail::resolve_resource_file(rel);
    if (path == nullptr) {
        return Texture2D{};
    }
    Texture2D tex = LoadTexture(path);
    if (tex.id != 0U) {
        SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    } else {
        TraceLog(LOG_WARNING, "[zh] LoadTexture failed: %s", path);
    }
    return tex;
}

void draw_tex_pro(Texture2D const &tex, Rectangle const src, Rectangle const dest) noexcept {
    if (tex.id == 0U) {
        DrawRectangleRec(dest, Color{160, 164, 170, 255});
        return;
    }
    DrawTexturePro(tex, src, dest, Vector2{}, 0.f, WHITE);
}

void draw_tex_stretch(Texture2D const &tex, Rectangle const dest) noexcept {
    if (tex.id == 0U) {
        DrawRectangleRec(dest, Color{160, 164, 170, 255});
        return;
    }
    Rectangle const src{0.f, 0.f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
    DrawTexturePro(tex, src, dest, Vector2{}, 0.f, WHITE);
}

[[nodiscard]] Color parse_css_color(std::string const &text) noexcept {
    if (text.empty()) {
        return Color{0, 0, 0, 0};
    }

    auto read_css_channels = [](std::string const &src,
                                std::size_t start,
                                bool alpha_is_unit) -> std::optional<Color> {
        std::size_t i = start;
        float channels[4]{};
        for (int c = 0; c < 4; ++c) {
            while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) {
                ++i;
            }
            if (i >= src.size()) {
                return std::nullopt;
            }
            char *end = nullptr;
            float const v = std::strtof(src.c_str() + i, &end);
            if (end == src.c_str() + i) {
                return std::nullopt;
            }
            channels[c] = v;
            i = static_cast<std::size_t>(end - src.c_str());
            if (c < 3) {
                while (i < src.size() && src[i] != ',' && src[i] != ')') {
                    ++i;
                }
                if (i >= src.size() || src[i] != ',') {
                    return std::nullopt;
                }
                ++i;
            }
        }
        unsigned char const r =
            static_cast<unsigned char>(std::clamp(channels[0], 0.f, 255.f));
        unsigned char const g =
            static_cast<unsigned char>(std::clamp(channels[1], 0.f, 255.f));
        unsigned char const b =
            static_cast<unsigned char>(std::clamp(channels[2], 0.f, 255.f));
        float a = channels[3];
        if (alpha_is_unit && a <= 1.f) {
            a *= 255.f;
        }
        unsigned char const alpha =
            static_cast<unsigned char>(std::clamp(a, 0.f, 255.f));
        return Color{r, g, b, alpha};
    };

    if (text.rfind("rgba(", 0) == 0) {
        if (auto c = read_css_channels(text, 5, true)) {
            return *c;
        }
    }
    if (text.rfind("rgb(", 0) == 0) {
        std::size_t i = 4;
        float channels[3]{};
        for (int c = 0; c < 3; ++c) {
            while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
                ++i;
            }
            char *end = nullptr;
            float const v = std::strtof(text.c_str() + i, &end);
            if (end == text.c_str() + i) {
                break;
            }
            channels[c] = v;
            i = static_cast<std::size_t>(end - text.c_str());
            if (c < 2) {
                while (i < text.size() && text[i] != ',') {
                    ++i;
                }
                if (i >= text.size() || text[i] != ',') {
                    break;
                }
                ++i;
            }
        }
        return Color{static_cast<unsigned char>(std::clamp(channels[0], 0.f, 255.f)),
                     static_cast<unsigned char>(std::clamp(channels[1], 0.f, 255.f)),
                     static_cast<unsigned char>(std::clamp(channels[2], 0.f, 255.f)), 255};
    }

    if (text[0] == '#') {
        auto hex_val = [](char c) -> int {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return 10 + (c - 'a');
            }
            if (c >= 'A' && c <= 'F') {
                return 10 + (c - 'A');
            }
            return 0;
        };
        std::string hex = text.substr(1);
        if (hex.size() == 3) {
            int const r = hex_val(hex[0]) * 17;
            int const g = hex_val(hex[1]) * 17;
            int const b = hex_val(hex[2]) * 17;
            return Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                           static_cast<unsigned char>(b), 255};
        }
        if (hex.size() >= 6) {
            int const r = hex_val(hex[0]) * 16 + hex_val(hex[1]);
            int const g = hex_val(hex[2]) * 16 + hex_val(hex[3]);
            int const b = hex_val(hex[4]) * 16 + hex_val(hex[5]);
            unsigned char a = 255;
            if (hex.size() >= 8) {
                a = static_cast<unsigned char>(hex_val(hex[6]) * 16 + hex_val(hex[7]));
            }
            return Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                           static_cast<unsigned char>(b), a};
        }
    }
    return Color{0, 0, 0, 0};
}

[[nodiscard]] float clamped_corner_radius(RectF const &rect, float corner_radius) noexcept {
    if (corner_radius <= 1e-3f) {
        return 0.f;
    }
    return std::min({corner_radius, rect.w * 0.5f, rect.h * 0.5f});
}

// Raylib roundness is 0..1; editor stores absolute corner radius in world units.
[[nodiscard]] float rect_roundness(RectF const &rect, float corner_radius) noexcept {
    float const cr = clamped_corner_radius(rect, corner_radius);
    if (cr <= 1e-3f) {
        return 0.f;
    }
    float const ref = std::min(rect.w, rect.h);
    if (ref <= 1e-3f) {
        return 0.f;
    }
    return std::clamp((2.f * cr) / ref, 0.f, 1.f);
}

void draw_tiled_texture(Rectangle const dest,
                        Texture2D const &tex,
                        float world_per_tex) noexcept {
    if (tex.id == 0U || dest.width <= 0.f || dest.height <= 0.f) {
        return;
    }
    Rectangle const src{0.f, 0.f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
    float const tile_w = src.width * world_per_tex;
    float const tile_h = src.height * world_per_tex;
    for (float y = dest.y; y < dest.y + dest.height; y += tile_h) {
        float const h = std::min(tile_h, dest.y + dest.height - y);
        for (float x = dest.x; x < dest.x + dest.width; x += tile_w) {
            float const w = std::min(tile_w, dest.x + dest.width - x);
            draw_tex_pro(tex, src, Rectangle{x, y, w, h});
        }
    }
}

struct BackgroundTextureCache {
    std::string path;
    Texture2D tex{};
};

BackgroundTextureCache &background_texture_cache() noexcept {
    static BackgroundTextureCache cache{};
    return cache;
}

void draw_map_background(MapDefinition const &map, Texture2D const &ice_fallback) noexcept {
    if (!map.background.has_value()) {
        return;
    }
    MapBackground const &bg = *map.background;
    RectF dest = bg.dest;
    if (dest.w <= 0.f || dest.h <= 0.f) {
        dest = map.camera_bounds;
    }
    Rectangle const dest_r{dest.x, dest.y, dest.w, dest.h};

    if (bg.color.has_value()) {
        DrawRectangleRec(dest_r, parse_css_color(*bg.color));
    }

    if (!bg.image.has_value() || bg.image->empty()) {
        return;
    }

    BackgroundTextureCache &cache = background_texture_cache();
    if (cache.path != *bg.image) {
        if (cache.tex.id != 0U) {
            UnloadTexture(cache.tex);
        }
        cache.tex = try_load(bg.image->c_str());
        cache.path = *bg.image;
    }
    if (cache.tex.id == 0U) {
        float const wpt = map.world_per_tex > 1e-3f ? map.world_per_tex : kWorldPerTex;
        if (ice_fallback.id != 0U) {
            draw_tiled_texture(dest_r, ice_fallback, wpt);
        } else if (!bg.color.has_value()) {
            DrawRectangleRec(dest_r, Color{172, 176, 182, 255});
        }
        return;
    }

    Rectangle src{0.f, 0.f, static_cast<float>(cache.tex.width),
                  static_cast<float>(cache.tex.height)};
    if (bg.use_src && bg.src.w > 0.f && bg.src.h > 0.f) {
        src = Rectangle{bg.src.x, bg.src.y, bg.src.w, bg.src.h};
    }

    if (bg.repeat) {
        for (float y = dest_r.y; y < dest_r.y + dest_r.height; y += src.height) {
            float const dh = std::min(src.height, dest_r.y + dest_r.height - y);
            for (float x = dest_r.x; x < dest_r.x + dest_r.width; x += src.width) {
                float const dw = std::min(src.width, dest_r.x + dest_r.width - x);
                draw_tex_pro(cache.tex, src, Rectangle{x, y, dw, dh});
            }
        }
        return;
    }

    draw_tex_pro(cache.tex, src, dest_r);
}

void draw_src_tiled_x(Texture2D const &tex,
                      Rectangle const src_strip,
                      float const x0,
                      float const y,
                      float const x1,
                      float const dest_h) noexcept {
    if (tex.id == 0U || x1 <= x0 || src_strip.width <= 0.f) {
        return;
    }
    float const tile_w = src_strip.width * kWorldPerTex;
    for (float x = x0; x < x1 - 0.5f; x += tile_w) {
        float const w = std::min(tile_w, x1 - x);
        draw_tex_pro(tex, src_strip, Rectangle{x, y, w, dest_h});
    }
}

void draw_src_tiled_y(Texture2D const &tex,
                      Rectangle const src_strip,
                      float const x,
                      float const y0,
                      float const y1,
                      float const dest_w) noexcept {
    if (tex.id == 0U || y1 <= y0 || src_strip.height <= 0.f) {
        return;
    }
    float const tile_h = src_strip.height * kWorldPerTex;
    for (float y = y0; y < y1 - 0.5f; y += tile_h) {
        float const h = std::min(tile_h, y1 - y);
        draw_tex_pro(tex, src_strip, Rectangle{x, y, dest_w, h});
    }
}

void draw_nine_slice_walls(Texture2D const &walls, Rectangle const dest) noexcept {
    if (walls.id == 0U) {
        DrawRectangleLinesEx(dest, 3.f, Color{90, 220, 240, 255});
        return;
    }

    float const tw = static_cast<float>(walls.width);
    float const th = static_cast<float>(walls.height);

    float const sl = kWallSliceL;
    float const st = kWallSliceT;
    float const sr = kWallSliceR;
    float const sb = kWallSliceB;

    float const wsl = sl * kWorldPerTex;
    float const wst = st * kWorldPerTex;
    float const wsr = sr * kWorldPerTex;
    float const wsb = sb * kWorldPerTex;

    float const dl = dest.x;
    float const dt = dest.y;
    float const dr = dest.x + dest.width;
    float const db = dest.y + dest.height;

    Rectangle const src_tl{0.f, 0.f, sl, st};
    Rectangle const src_tr{tw - sr, 0.f, sr, st};
    Rectangle const src_bl{0.f, th - sb, sl, sb};
    Rectangle const src_br{tw - sr, th - sb, sr, sb};

    Rectangle const src_top{sl, 0.f, tw - sl - sr, st};
    Rectangle const src_bot{sl, th - sb, tw - sl - sr, sb};
    Rectangle const src_left{0.f, st, sl, th - st - sb};
    Rectangle const src_right{tw - sr, st, sr, th - st - sb};

    draw_tex_pro(walls, src_tl, Rectangle{dl, dt, wsl, wst});
    draw_tex_pro(walls, src_tr, Rectangle{dr - wsr, dt, wsr, wst});
    draw_tex_pro(walls, src_bl, Rectangle{dl, db - wsb, wsl, wsb});
    draw_tex_pro(walls, src_br, Rectangle{dr - wsr, db - wsb, wsr, wsb});

    draw_src_tiled_x(walls, src_top, dl + wsl, dt, dr - wsr, wst);
    draw_src_tiled_x(walls, src_bot, dl + wsl, db - wsb, dr - wsr, wsb);
    draw_src_tiled_y(walls, src_left, dl, dt + wst, db - wsb, wsl);
    draw_src_tiled_y(walls, src_right, dr - wsr, dt + wst, db - wsb, wsr);
}

void draw_ice_tiled(RinkSpriteSet const &sprites) noexcept {
    float const tile_w = sprites.ice.id != 0U ? static_cast<float>(sprites.ice.width) * kWorldPerTex
                                              : kIceTileWorld;
    float const tile_h = sprites.ice.id != 0U ? static_cast<float>(sprites.ice.height) * kWorldPerTex
                                              : kIceTileWorld;

    for (float y = rink_min_y(); y < rink_max_y(); y += tile_h) {
        float const h = std::min(tile_h, rink_max_y() - y);
        for (float x = rink_min_x(); x < rink_max_x(); x += tile_w) {
            float const w = std::min(tile_w, rink_max_x() - x);
            if (sprites.ice.id != 0U) {
                draw_tex_stretch(sprites.ice, Rectangle{x, y, w, h});
            } else {
                DrawRectangleRec(Rectangle{x, y, w, h}, Color{172, 176, 182, 255});
            }
        }
    }
}

void draw_faceoff_circle(Texture2D const &circle, Vector2 const center, float const diameter) noexcept {
    if (circle.id == 0U) {
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                        diameter * 0.5f, Color{18, 20, 24, 255});
        return;
    }

    Rectangle const src{kCircleContentL, kCircleContentT, kCircleContentR - kCircleContentL,
                        kCircleContentB - kCircleContentT};
    draw_tex_pro(circle, src,
                 Rectangle{center.x - diameter * 0.5f, center.y - diameter * 0.5f, diameter,
                           diameter});
}

void draw_rink_markings(RinkSpriteSet const &sprites) noexcept {
    Color const line_black{18, 20, 24, 255};
    Color const crease_red{118, 42, 48, 255};
    float const cx = rink_center_x();
    float const cy = rink_mid_y();

    DrawLineEx({cx, rink_min_y() + 20.f}, {cx, rink_max_y() - 20.f}, 4.f, line_black);

    float const goal_inset = (rink_max_x() - rink_min_x()) * 0.058f;
    float const lg = rink_min_x() + goal_inset;
    float const rg = rink_max_x() - goal_inset;
    DrawLineEx({lg, rink_min_y() + 18.f}, {lg, rink_max_y() - 18.f}, 3.5f, line_black);
    DrawLineEx({rg, rink_min_y() + 18.f}, {rg, rink_max_y() - 18.f}, 3.5f, line_black);

    float const zone_inset = (rink_max_x() - rink_min_x()) * 0.22f;
    DrawLineEx({rink_min_x() + zone_inset, rink_min_y() + 18.f}, {rink_min_x() + zone_inset, rink_max_y() - 18.f},
               2.5f, line_black);
    DrawLineEx({rink_max_x() - zone_inset, rink_min_y() + 18.f}, {rink_max_x() - zone_inset, rink_max_y() - 18.f},
               2.5f, line_black);

    draw_faceoff_circle(sprites.circle, {cx, cy}, 176.f);

    float const fo_r = 58.f;
    float const fo_d = fo_r * 2.f;
    float const fo_x = (cx - lg) * 0.52f;
    float const fo_y = (rink_max_y() - rink_min_y()) * 0.26f;
    Vector2 const faceoffs[4] = {
        {lg + fo_x, cy - fo_y},
        {lg + fo_x, cy + fo_y},
        {rg - fo_x, cy - fo_y},
        {rg - fo_x, cy + fo_y},
    };
    for (Vector2 const fc : faceoffs) {
        draw_faceoff_circle(sprites.circle, fc, fo_d);
    }

    DrawCircleSectorLines({lg, cy}, 50.f, -90.f, 90.f, 32, crease_red);
    DrawCircleSectorLines({rg, cy}, 50.f, 90.f, 270.f, 32, crease_red);
}

void draw_goal_h3slice(Texture2D const &goal,
                       Rectangle const dest,
                       bool const flip_x) noexcept {
    if (goal.id == 0U) {
        DrawRectangleRec(dest, Color{120, 40, 40, 220});
        return;
    }

    float const content_w = kGoalContentR - kGoalContentL;
    float const content_h = kGoalContentB - kGoalContentT;
    float const mid_src_w = content_w - kGoalSliceL - kGoalSliceR;

    float const post_w = dest.width * (kGoalSliceL / content_w);
    float const mid_w = dest.width - post_w * 2.f;

    Rectangle const src_left{kGoalContentL, kGoalContentT, kGoalSliceL, content_h};
    Rectangle const src_mid{kGoalContentL + kGoalSliceL, kGoalContentT, mid_src_w, content_h};
    Rectangle const src_right{kGoalContentR - kGoalSliceR, kGoalContentT, kGoalSliceR, content_h};

    Rectangle dest_left{dest.x, dest.y, post_w, dest.height};
    Rectangle dest_mid{dest.x + post_w, dest.y, mid_w, dest.height};
    Rectangle dest_right{dest.x + post_w + mid_w, dest.y, post_w, dest.height};

    if (!flip_x) {
        draw_tex_pro(goal, src_left, dest_left);
        draw_src_tiled_x(goal, src_mid, dest_mid.x, dest_mid.y, dest_mid.x + dest_mid.width,
                         dest_mid.height);
        draw_tex_pro(goal, src_right, dest_right);
        return;
    }

    auto flip_src = [](Rectangle const r, float const tex_w) {
        return Rectangle{tex_w - r.x - r.width, r.y, r.width, r.height};
    };
    float const tw = static_cast<float>(goal.width);
    draw_tex_pro(goal, flip_src(src_right, tw), dest_left);
    draw_src_tiled_x(goal, flip_src(src_mid, tw), dest_mid.x, dest_mid.y, dest_mid.x + dest_mid.width,
                     dest_mid.height);
    draw_tex_pro(goal, flip_src(src_left, tw), dest_right);
}

void draw_goals(RinkSpriteSet const &sprites) noexcept {
    RectF const gl = left_goal_zone();
    RectF const gr = right_goal_zone();
    if (gl.w <= 0.f && gl.h <= 0.f && gr.w <= 0.f && gr.h <= 0.f) {
        return;
    }

    float const content_h = kGoalContentB - kGoalContentT;
    float const content_w = kGoalContentR - kGoalContentL;
    float const gh = std::max(gl.h * 1.35f, 190.f);
    float const scale = gh / (content_h * kWorldPerTex);
    float const gw = content_w * kWorldPerTex * scale;

    draw_goal_h3slice(sprites.goal,
                      Rectangle{gl.x + gl.w - gw, gl.y + (gl.h - gh) * 0.5f, gw, gh}, true);
    draw_goal_h3slice(sprites.goal, Rectangle{gr.x, gr.y + (gr.h - gh) * 0.5f, gw, gh}, false);
}

void draw_visual_piece(RinkSpriteSet const &sprites, MapVisualPiece const &piece) noexcept {
    Rectangle const dest{piece.dest.x, piece.dest.y, piece.dest.w, piece.dest.h};
    switch (piece.kind) {
        case MapVisualKind::IceTile: {
            Rectangle src{0.f, 0.f, static_cast<float>(sprites.ice.width),
                          static_cast<float>(sprites.ice.height)};
            if (piece.use_src) {
                src = Rectangle{piece.src.x, piece.src.y, piece.src.w, piece.src.h};
            }
            float const tile_w = src.width * kWorldPerTex;
            float const tile_h = src.height * kWorldPerTex;
            for (float y = dest.y; y < dest.y + dest.height; y += tile_h) {
                float const h = std::min(tile_h, dest.y + dest.height - y);
                for (float x = dest.x; x < dest.x + dest.width; x += tile_w) {
                    float const w = std::min(tile_w, dest.x + dest.width - x);
                    draw_tex_pro(sprites.ice, src, Rectangle{x, y, w, h});
                }
            }
            break;
        }
        case MapVisualKind::WallFrame:
            draw_nine_slice_walls(sprites.walls, dest);
            break;
        case MapVisualKind::CircleMark: {
            Rectangle src{kCircleContentL, kCircleContentT, kCircleContentR - kCircleContentL,
                          kCircleContentB - kCircleContentT};
            if (piece.use_src) {
                src = Rectangle{piece.src.x, piece.src.y, piece.src.w, piece.src.h};
            }
            draw_tex_pro(sprites.circle, src, dest);
            break;
        }
        case MapVisualKind::GoalSprite:
            draw_goal_h3slice(sprites.goal, dest, piece.flip_x);
            break;
    }
}

void draw_styled_round_rect(RectF const &rect,
                            float corner_radius,
                            std::optional<std::string> const &fill,
                            std::optional<std::string> const &stroke,
                            float stroke_width) noexcept {
    Rectangle const r{rect.x, rect.y, rect.w, rect.h};
    float const roundness = rect_roundness(rect, corner_radius);
    int const segments = 12;
    if (fill.has_value()) {
        Color const fill_c = parse_css_color(*fill);
        if (fill_c.a > 0) {
            if (roundness > 1e-3f) {
                DrawRectangleRounded(r, roundness, segments, fill_c);
            } else {
                DrawRectangleRec(r, fill_c);
            }
        }
    }
    if (stroke.has_value()) {
        Color const stroke_c = parse_css_color(*stroke);
        if (stroke_c.a > 0) {
            float const lw = std::max(1.f, stroke_width);
            if (roundness > 1e-3f) {
                DrawRectangleRoundedLinesEx(r, roundness, segments, lw, stroke_c);
            } else {
                DrawRectangleLinesEx(r, lw, stroke_c);
            }
        }
    }
}

float norm_angle(float a) noexcept {
    constexpr float kTau = 6.28318530718f;
    a = std::fmod(a, kTau);
    if (a < 0.f) {
        a += kTau;
    }
    return a;
}

void draw_map_arc_stroke(MapArc3 const &arc, Color stroke, float stroke_width) noexcept {
    ArcGeom g{};
    if (!arc_from_three_points(arc, g)) {
        return;
    }
    float const lw = std::max(1.f, stroke_width);
    int const segments = 32;
    float const start = g.a0;
    float const span = g.ccw ? norm_angle(g.a1 - g.a0) : norm_angle(g.a0 - g.a1);
    Vector2 prev{g.cx + g.r * std::cos(start), g.cy + g.r * std::sin(start)};
    for (int i = 1; i <= segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const ang = g.ccw ? start + t * span : start - t * span;
        Vector2 const cur{g.cx + g.r * std::cos(ang), g.cy + g.r * std::sin(ang)};
        DrawLineEx(prev, cur, lw, stroke);
        prev = cur;
    }
}

void draw_map_shape(MapShape const &shape) noexcept {
    switch (shape.kind) {
        case MapShapeKind::Rect:
            draw_styled_round_rect(shape.rect, shape.corner_radius, shape.fill, shape.stroke,
                                   shape.stroke_width);
            break;
        case MapShapeKind::Circle: {
            if (shape.fill.has_value()) {
                Color const fill_c = parse_css_color(*shape.fill);
                if (fill_c.a > 0) {
                    DrawCircleV({shape.x, shape.y}, shape.radius, fill_c);
                }
            }
            if (shape.stroke.has_value()) {
                Color const stroke_c = parse_css_color(*shape.stroke);
                if (stroke_c.a > 0) {
                    float const w = std::max(1.f, shape.stroke_width);
                    float const half = w * 0.5f;
                    DrawRing({shape.x, shape.y}, std::max(0.f, shape.radius - half),
                             shape.radius + half, 0.f, 360.f, 64, stroke_c);
                }
            }
            break;
        }
        case MapShapeKind::Line:
            if (shape.stroke.has_value()) {
                Color const stroke_c = parse_css_color(*shape.stroke);
                if (stroke_c.a > 0) {
                    DrawLineEx({shape.line_p1.x, shape.line_p1.y},
                               {shape.line_p2.x, shape.line_p2.y},
                               std::max(1.f, shape.stroke_width), stroke_c);
                }
            }
            break;
        case MapShapeKind::Arc:
            if (shape.fill.has_value()) {
                ArcGeom g{};
                if (arc_from_three_points(shape.arc, g)) {
                    Color const fill_c = parse_css_color(*shape.fill);
                    if (fill_c.a > 0) {
                        float const start_deg = g.a0 * RAD2DEG;
                        float const end_deg = g.a1 * RAD2DEG;
                        DrawCircleSector({g.cx, g.cy}, g.r, start_deg, end_deg, 32, fill_c);
                    }
                }
            }
            if (shape.stroke.has_value()) {
                Color const stroke_c = parse_css_color(*shape.stroke);
                if (stroke_c.a > 0) {
                    draw_map_arc_stroke(shape.arc, stroke_c, shape.stroke_width);
                }
            }
            break;
    }
}

enum class MapLayerKind : std::uint8_t { Visual, Shape };

struct MapLayerEntry {
    MapLayerKind kind{};
    std::size_t index{};
    int z{};
};

[[nodiscard]] bool map_skips_baked_markings(MapDefinition const &map) noexcept {
    if (!map.shapes.empty()) {
        return true;
    }
    if (map.background.has_value()) {
        return true;
    }
    return map.visuals.empty() && map_runtime().loaded_from_file();
}

// Walk MapDefinition visuals/shapes in z-order; fall back to baked layout when map file is empty.
void draw_map_visuals(RinkSpriteSet const &sprites, MapDefinition const &map) noexcept {
    draw_map_background(map, sprites.ice);

    std::vector<MapLayerEntry> layers;
    layers.reserve(map.visuals.size() + map.shapes.size());
    for (std::size_t i = 0; i < map.visuals.size(); ++i) {
        MapVisualPiece const &piece = map.visuals[i];
        int const z = piece.z.has_value() ? *piece.z : static_cast<int>(i);
        layers.push_back(MapLayerEntry{MapLayerKind::Visual, i, z});
    }
    for (std::size_t i = 0; i < map.shapes.size(); ++i) {
        MapShape const &shape = map.shapes[i];
        int const z = shape.z.has_value() ? *shape.z : static_cast<int>(map.visuals.size() + i);
        layers.push_back(MapLayerEntry{MapLayerKind::Shape, i, z});
    }
    std::sort(layers.begin(), layers.end(), [](MapLayerEntry const &a, MapLayerEntry const &b) {
        if (a.z != b.z) {
            return a.z < b.z;
        }
        if (a.kind != b.kind) {
            return a.kind < b.kind;
        }
        return a.index < b.index;
    });
    for (MapLayerEntry const &layer : layers) {
        if (layer.kind == MapLayerKind::Visual) {
            draw_visual_piece(sprites, map.visuals[layer.index]);
        } else {
            draw_map_shape(map.shapes[layer.index]);
        }
    }

    if (!map_skips_baked_markings(map)) {
        draw_rink_markings(sprites);
    }
}

}  // namespace

// Load resources/rink/{ice,walls,circle,goal}.png; ready=false if any texture fails.
void load_rink_sprites(RinkSpriteSet &out) noexcept {
    unload_rink_sprites(out);
    out.ice = try_load("rink/ice.png");
    out.walls = try_load("rink/walls.png");
    out.circle = try_load("rink/circle.png");
    out.goal = try_load("rink/goal.png");
    out.ready = out.ice.id != 0U && out.walls.id != 0U && out.circle.id != 0U && out.goal.id != 0U;
    if (!out.ready) {
        TraceLog(LOG_WARNING, "[zh] Rink sprites incomplete — expected resources/rink/{ice,walls,circle,goal}.png");
    } else {
        TraceLog(LOG_INFO, "[zh] Loaded rink sprites from resources/rink/");
    }
}

void unload_rink_sprites(RinkSpriteSet &sprites) noexcept {
    if (sprites.ice.id != 0U) {
        UnloadTexture(sprites.ice);
    }
    if (sprites.walls.id != 0U) {
        UnloadTexture(sprites.walls);
    }
    if (sprites.circle.id != 0U) {
        UnloadTexture(sprites.circle);
    }
    if (sprites.goal.id != 0U) {
        UnloadTexture(sprites.goal);
    }
    sprites = RinkSpriteSet{};
}

// Playing-screen backdrop from current map_runtime().definition().
void draw_modular_rink(RinkSpriteSet const &sprites) noexcept {
    draw_map_visuals(sprites, map_runtime().definition());
}

}  // namespace zh::ui
