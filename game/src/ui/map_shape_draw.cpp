#include "zh/ui/map_shape_draw.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/map_geometry.hpp"
#include "zh/ui/world_space_3d.hpp"

#include "zh/gfx/gfx_compat.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace zh::ui {

namespace {

using namespace zh::game;

inline constexpr float kIceSurfaceY = kPlayingIceY;
inline constexpr float kMarkSurfaceY = kPlayingMarkY;
inline constexpr float kSurroundY = -8.f;
inline constexpr float kStadiumApronY = -14.f;
inline constexpr float kIceSlabThickness = 8.f;
inline constexpr float kFloorSlabThickness = 16.f;
inline constexpr float kBoardWallThickness = 10.f;
inline constexpr float kStadiumWallThickness = 28.f;
inline constexpr float kWallHeight = 44.f;
inline constexpr float kWallBaseY = 0.15f;
inline constexpr float kWallCapY = kWallHeight + 0.8f;
inline constexpr float kDefaultIceColor[] = {228.f, 236.f, 244.f, 255.f};

struct BoardOutline {
    RectF rect{};
    float corner_radius{0.f};
};

[[nodiscard]] Vector3 sim_point(float sim_x, float sim_y, float y) noexcept {
    return sim_to_world3(sim_x, sim_y, y);
}

[[nodiscard]] float clamped_corner_radius(RectF const &rect, float corner_radius) noexcept {
    if (corner_radius <= 1e-3f) {
        return 0.f;
    }
    return std::min({corner_radius, rect.w * 0.5f, rect.h * 0.5f});
}

[[nodiscard]] float norm_angle(float a) noexcept {
    constexpr float kTau = 6.28318530718f;
    a = std::fmod(a, kTau);
    if (a < 0.f) {
        a += kTau;
    }
    return a;
}

void draw_horizontal_quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) noexcept {
    // +Y normal (ice and painted markings).
    DrawTriangle3D(a, c, b, color);
    DrawTriangle3D(a, d, c, color);
}

void draw_ground_quad(float x0,
                      float y0,
                      float x1,
                      float y1,
                      float x2,
                      float y2,
                      float x3,
                      float y3,
                      Color color,
                      float y = kMarkSurfaceY) noexcept {
    draw_horizontal_quad(sim_point(x0, y0, y), sim_point(x1, y1, y), sim_point(x2, y2, y),
                         sim_point(x3, y3, y), color);
}

void draw_ground_disc(float cx, float cy, float radius, Color color, int segments = 32) noexcept {
    if (radius <= 1e-3f) {
        return;
    }
    segments = std::max(segments, 12);
    Vector3 const center = sim_point(cx, cy, kMarkSurfaceY);
    constexpr float kTau = 6.28318530718f;
    for (int i = 0; i < segments; ++i) {
        float const t0 = (static_cast<float>(i) / static_cast<float>(segments)) * kTau;
        float const t1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * kTau;
        Vector3 const p0 =
            sim_point(cx + radius * std::cos(t0), cy + radius * std::sin(t0), kMarkSurfaceY);
        Vector3 const p1 =
            sim_point(cx + radius * std::cos(t1), cy + radius * std::sin(t1), kMarkSurfaceY);
        DrawTriangle3D(center, p0, p1, color);
    }
}

void draw_ground_annulus_marking(float cx,
                                 float cy,
                                 float radius,
                                 float width,
                                 Color color,
                                 int segments = 64) noexcept {
    if (radius <= 1e-3f || width <= 1e-3f) {
        return;
    }
    float const r_in = std::max(radius - width * 0.5f, 0.f);
    float const r_out = radius + width * 0.5f;
    if (r_out <= r_in + 1e-3f) {
        draw_ground_disc(cx, cy, r_out, color, segments);
        return;
    }

    segments = std::max(segments, 24);
    constexpr float kTau = 6.28318530718f;
    for (int i = 0; i < segments; ++i) {
        float const t0 = (static_cast<float>(i) / static_cast<float>(segments)) * kTau;
        float const t1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * kTau;
        float const c0 = std::cos(t0);
        float const s0 = std::sin(t0);
        float const c1 = std::cos(t1);
        float const s1 = std::sin(t1);
        draw_ground_quad(cx + r_in * c0, cy + r_in * s0, cx + r_in * c1, cy + r_in * s1,
                         cx + r_out * c1, cy + r_out * s1, cx + r_out * c0, cy + r_out * s0,
                         color);
    }
}

void draw_ice_surface_rect(RectF const &rect, Color color, float y = kIceSurfaceY) noexcept {
    if (rect.w <= 0.f || rect.h <= 0.f) {
        return;
    }
    float const thickness = (y >= kIceSurfaceY - 0.01f) ? kIceSlabThickness : kFloorSlabThickness;
    float const cx = rect.x + rect.w * 0.5f;
    float const cz = rect.y + rect.h * 0.5f;
    float const cy = y - thickness * 0.5f;
    DrawCube(sim_point(cx, cz, cy), rect.w, thickness, rect.h, color);
}

void draw_quad_face(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) noexcept {
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(a, c, d, color);
}

void draw_solid_wall_segment(float ax,
                             float ay,
                             float bx,
                             float by,
                             float y_bottom,
                             float y_top,
                             float thickness,
                             float ref_x,
                             float ref_y,
                             Color color) noexcept {
    if (y_top <= y_bottom + 1e-3f || thickness <= 1e-3f) {
        return;
    }
    float const dx = bx - ax;
    float const dy = by - ay;
    float const len = std::hypot(dx, dy);
    if (len <= 1e-3f) {
        return;
    }

    float nx = -dy / len;
    float ny = dx / len;
    float const mx = (ax + bx) * 0.5f;
    float const my = (ay + by) * 0.5f;
    if (nx * (mx - ref_x) + ny * (my - ref_y) < 0.f) {
        nx = -nx;
        ny = -ny;
    }

    float const ox = nx * thickness * 0.5f;
    float const oy = ny * thickness * 0.5f;

    Vector3 const a_in_b = sim_point(ax - ox, ay - oy, y_bottom);
    Vector3 const b_in_b = sim_point(bx - ox, by - oy, y_bottom);
    Vector3 const a_out_b = sim_point(ax + ox, ay + oy, y_bottom);
    Vector3 const b_out_b = sim_point(bx + ox, by + oy, y_bottom);
    Vector3 const a_in_t = sim_point(ax - ox, ay - oy, y_top);
    Vector3 const b_in_t = sim_point(bx - ox, by - oy, y_top);
    Vector3 const a_out_t = sim_point(ax + ox, ay + oy, y_top);
    Vector3 const b_out_t = sim_point(bx + ox, by + oy, y_top);

    draw_quad_face(a_out_b, b_out_b, b_out_t, a_out_t, color);
    draw_quad_face(b_in_b, a_in_b, a_in_t, b_in_t, color);
    draw_quad_face(a_in_t, b_in_t, b_out_t, a_out_t, color);
    draw_quad_face(a_in_b, a_out_b, b_out_b, b_in_b, color);
    draw_quad_face(a_in_b, a_out_b, a_out_t, a_in_t, color);
    draw_quad_face(b_out_b, b_in_b, b_in_t, b_out_t, color);
}

void draw_axis_aligned_wall_slab(float center_x,
                                 float center_z,
                                 float size_x,
                                 float size_z,
                                 float height_y,
                                 Color color) noexcept {
    if (height_y <= 1e-3f || size_x <= 1e-3f || size_z <= 1e-3f) {
        return;
    }
    DrawCube(sim_point(center_x, center_z, height_y * 0.5f), size_x, height_y, size_z, color);
}

void draw_ground_line_marking(float x1,
                              float y1,
                              float x2,
                              float y2,
                              float width,
                              Color color) noexcept {
    float const dx = x2 - x1;
    float const dy = y2 - y1;
    float const len = std::sqrt(dx * dx + dy * dy);
    if (len <= 1e-3f) {
        return;
    }
    float const nx = -dy / len * width * 0.5f;
    float const ny = dx / len * width * 0.5f;
    draw_ground_quad(x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny,
                     color);
}

void draw_ground_ring_marking(float cx,
                              float cy,
                              float radius,
                              float width,
                              Color color,
                              int segments = 64) noexcept {
    draw_ground_annulus_marking(cx, cy, radius, width, color, segments);
}

void draw_ground_dot(float cx, float cy, float radius, Color color) noexcept {
    draw_ground_disc(cx, cy, radius, color, radius > 24.f ? 32 : 20);
}

void collect_round_rect_points(RectF const &rect,
                               float corner_radius,
                               int arc_segments,
                               std::vector<Vector2> &out) noexcept {
    float const cr = clamped_corner_radius(rect, corner_radius);
    float const x0 = rect.x;
    float const y0 = rect.y;
    float const x1 = rect.x + rect.w;
    float const y1 = rect.y + rect.h;
    out.clear();

    auto push = [&](float x, float y) { out.push_back(Vector2{x, y}); };

    if (cr <= 1e-3f) {
        push(x0, y0);
        push(x1, y0);
        push(x1, y1);
        push(x0, y1);
        return;
    }

    arc_segments = std::max(arc_segments, 4);
    auto arc = [&](float cx, float cy, float start_deg, float end_deg) {
        float const start = start_deg * DEG2RAD;
        float const end = end_deg * DEG2RAD;
        for (int i = 0; i <= arc_segments; ++i) {
            float const t = static_cast<float>(i) / static_cast<float>(arc_segments);
            float const ang = start + (end - start) * t;
            push(cx + cr * std::cos(ang), cy + cr * std::sin(ang));
        }
    };

    for (float x = x0 + cr; x <= x1 - cr; x += 18.f) {
        push(x, y0);
    }
    push(x1 - cr, y0);
    arc(x1 - cr, y0 + cr, 270.f, 360.f);
    for (float y = y0 + cr; y <= y1 - cr; y += 18.f) {
        push(x1, y);
    }
    push(x1, y1 - cr);
    arc(x1 - cr, y1 - cr, 0.f, 90.f);
    for (float x = x1 - cr; x >= x0 + cr; x -= 18.f) {
        push(x, y1);
    }
    push(x0 + cr, y1);
    arc(x0 + cr, y1 - cr, 90.f, 180.f);
    for (float y = y1 - cr; y >= y0 + cr; y -= 18.f) {
        push(x0, y);
    }
    push(x0, y0 + cr);
    arc(x0 + cr, y0 + cr, 180.f, 270.f);
}

[[nodiscard]] std::optional<BoardOutline> find_board_outline(MapDefinition const &map) noexcept {
    for (MapCollider const &c : map.colliders) {
        if (c.kind == MapColliderKind::Rect && c.corner_radius > 1e-3f && c.rect.w > 0.f &&
            c.rect.h > 0.f) {
            return BoardOutline{c.rect, c.corner_radius};
        }
    }

    BoardOutline best{};
    bool found = false;
    for (MapShape const &s : map.shapes) {
        if (s.kind == MapShapeKind::Rect && s.stroke.has_value() && s.corner_radius > 1e-3f &&
            s.rect.w > 0.f && s.rect.h > 0.f) {
            float const area = s.rect.w * s.rect.h;
            if (!found || area > best.rect.w * best.rect.h) {
                best = BoardOutline{s.rect, s.corner_radius};
                found = true;
            }
        }
    }
    if (found) {
        return best;
    }

    for (MapCollider const &c : map.colliders) {
        if (c.kind == MapColliderKind::Rect && c.rect.w > 0.f && c.rect.h > 0.f) {
            return BoardOutline{c.rect, c.corner_radius};
        }
    }

    if (map.player_bounds.w > 0.f && map.player_bounds.h > 0.f) {
        return BoardOutline{map.player_bounds, map.player_bounds.h * 0.5f};
    }
    return std::nullopt;
}

void draw_board_vertex_cap(float vx,
                           float vy,
                           float ref_x,
                           float ref_y,
                           float y_bottom,
                           float y_top,
                           float thickness,
                           Color color) noexcept {
    if (y_top <= y_bottom + 1e-3f || thickness <= 1e-3f) {
        return;
    }
    float dx = vx - ref_x;
    float dy = vy - ref_y;
    float const len = std::hypot(dx, dy);
    if (len <= 1e-3f) {
        return;
    }
    float const ox = dx / len * thickness * 0.5f;
    float const oy = dy / len * thickness * 0.5f;
    float const h = y_top - y_bottom;
    DrawCylinder(sim_point(vx + ox, vy + oy, y_bottom + h * 0.5f), thickness * 0.5f,
                 thickness * 0.5f, h, 16, color);
}

void draw_board_walls_3d(BoardOutline const &board) noexcept {
    Color const board_col{34, 42, 56, 255};
    Color const kick_col{22, 28, 38, 255};
    Color const cap_col{228, 236, 246, 255};

    std::vector<Vector2> pts;
    collect_round_rect_points(board.rect, board.corner_radius, 24, pts);
    if (pts.size() < 2) {
        return;
    }

    float const ref_x = board.rect.x + board.rect.w * 0.5f;
    float const ref_y = board.rect.y + board.rect.h * 0.5f;
    float const cap_thickness = kBoardWallThickness * 0.65f;
    float const kick_top = kWallBaseY + 3.5f;

    for (std::size_t i = 0; i < pts.size(); ++i) {
        Vector2 const &a = pts[i];
        Vector2 const &b = pts[(i + 1) % pts.size()];
        draw_solid_wall_segment(a.x, a.y, b.x, b.y, kWallBaseY, kick_top, kBoardWallThickness,
                                ref_x, ref_y, kick_col);
        draw_solid_wall_segment(a.x, a.y, b.x, b.y, kick_top, kWallHeight, kBoardWallThickness,
                                ref_x, ref_y, board_col);
        draw_solid_wall_segment(a.x, a.y, b.x, b.y, kWallHeight, kWallCapY, cap_thickness, ref_x,
                                ref_y, cap_col);
    }

    CapsuleBoardOutline const cap_outline = capsule_board_outline(board.rect);
    float const cr = clamped_corner_radius(board.rect, board.corner_radius);
    float const flat_l = board.rect.x + cr;
    float const flat_r = board.rect.x + board.rect.w - cr;
    float const y0 = board.rect.y;
    float const y1 = board.rect.y + board.rect.h;

    auto junction_cap = [&](float const x, float const y) {
        draw_board_vertex_cap(x, y, ref_x, ref_y, kWallBaseY, kick_top, kBoardWallThickness,
                              kick_col);
        draw_board_vertex_cap(x, y, ref_x, ref_y, kick_top, kWallHeight, kBoardWallThickness,
                              board_col);
        draw_board_vertex_cap(x, y, ref_x, ref_y, kWallHeight, kWallCapY, cap_thickness, cap_col);
    };

    if (cr >= board.rect.h * 0.5f - 1e-3f) {
        junction_cap(cap_outline.flat_left_x, cap_outline.min_y);
        junction_cap(cap_outline.flat_left_x, cap_outline.max_y);
        junction_cap(cap_outline.flat_right_x, cap_outline.min_y);
        junction_cap(cap_outline.flat_right_x, cap_outline.max_y);
    } else {
        junction_cap(flat_l, y0);
        junction_cap(flat_l, y1);
        junction_cap(flat_r, y0);
        junction_cap(flat_r, y1);
    }
}

void draw_goal_frame_3d(RectF const &zone, bool const west) noexcept {
    GoalEnclosureLayout const g = goal_enclosure_layout(zone, west);
    if (g.width() <= 1e-3f || g.depth() <= 1e-3f) {
        return;
    }

    Color const frame = west ? Color{48, 110, 230, 255} : Color{220, 58, 58, 255};
    Color const wall = ColorBrightness(frame, -0.10f);
    Color const wall_dark = ColorBrightness(frame, -0.22f);

    float const bar_y = kWallHeight - 10.f;
    float const wall_h = bar_y - kWallBaseY;
    float const wall_y = kWallBaseY + wall_h * 0.5f;
    float const mid_z = (g.top_y + g.bot_y) * 0.5f;
    float const mid_x = (g.mouth_x + g.back_x) * 0.5f;
    float const wall_t = kGoalEnclosureWallThickness;

    float const back_center_x =
        west ? g.back_x - wall_t * 0.5f : g.back_x + wall_t * 0.5f;
    DrawCube(sim_point(back_center_x, mid_z, wall_y), wall_t, wall_h, g.width(), wall_dark);
    DrawCube(sim_point(mid_x, g.top_y - wall_t * 0.5f, wall_y), g.depth(), wall_h, wall_t, wall);
    DrawCube(sim_point(mid_x, g.bot_y + wall_t * 0.5f, wall_y), g.depth(), wall_h, wall_t, wall);

    float const corner_r = kGoalCornerCylinderRadius;
    float const corner_h = wall_h + kGoalCornerCylinderHeightExtra;
    float const corner_y = kWallBaseY + corner_h * 0.5f;
    GoalEnclosureCorner corners[4]{};
    goal_enclosure_corners(g, west, corners);
    Color const corner_cols[4] = {wall_dark, wall_dark, frame, frame};
    for (int i = 0; i < 4; ++i) {
        DrawCylinder(sim_point(corners[i].x, corners[i].y, corner_y), corner_r, corner_r, corner_h,
                     18, corner_cols[i]);
    }
}

void draw_map_goals_3d(MapDefinition const &map) noexcept {
    if (map.goal_west.has_value()) {
        RectF const bounds = goal_zone_bounds(*map.goal_west);
        if (bounds.w > 0.f && bounds.h > 0.f) {
            draw_goal_frame_3d(bounds, true);
        }
    }
    if (map.goal_east.has_value()) {
        RectF const bounds = goal_zone_bounds(*map.goal_east);
        if (bounds.w > 0.f && bounds.h > 0.f) {
            draw_goal_frame_3d(bounds, false);
        }
    }
}

void draw_round_rect_stroke_marking(RectF const &rect,
                                    float corner_radius,
                                    Color stroke,
                                    float stroke_width) noexcept {
    float const cr = clamped_corner_radius(rect, corner_radius);
    float const x0 = rect.x;
    float const y0 = rect.y;
    float const x1 = rect.x + rect.w;
    float const y1 = rect.y + rect.h;

    if (cr <= 1e-3f) {
        draw_ground_line_marking(x0, y0, x1, y0, stroke_width, stroke);
        draw_ground_line_marking(x1, y0, x1, y1, stroke_width, stroke);
        draw_ground_line_marking(x1, y1, x0, y1, stroke_width, stroke);
        draw_ground_line_marking(x0, y1, x0, y0, stroke_width, stroke);
        return;
    }

    draw_ground_line_marking(x0 + cr, y0, x1 - cr, y0, stroke_width, stroke);
    draw_ground_line_marking(x1, y0 + cr, x1, y1 - cr, stroke_width, stroke);
    draw_ground_line_marking(x1 - cr, y1, x0 + cr, y1, stroke_width, stroke);
    draw_ground_line_marking(x0, y1 - cr, x0, y0 + cr, stroke_width, stroke);

    int const arc_segments = 24;
    auto arc = [&](float cx, float cy, float start_deg, float end_deg) {
        float const start = start_deg * DEG2RAD;
        float const end = end_deg * DEG2RAD;
        for (int i = 0; i < arc_segments; ++i) {
            float const t0 = static_cast<float>(i) / static_cast<float>(arc_segments);
            float const t1 = static_cast<float>(i + 1) / static_cast<float>(arc_segments);
            float const a0 = start + (end - start) * t0;
            float const a1 = start + (end - start) * t1;
            draw_ground_line_marking(cx + cr * std::cos(a0), cy + cr * std::sin(a0),
                                     cx + cr * std::cos(a1), cy + cr * std::sin(a1), stroke_width,
                                     stroke);
        }
    };

    arc(x1 - cr, y0 + cr, 270.f, 360.f);
    arc(x1 - cr, y1 - cr, 0.f, 90.f);
    arc(x0 + cr, y1 - cr, 90.f, 180.f);
    arc(x0 + cr, y0 + cr, 180.f, 270.f);
}

void draw_map_arc_stroke_marking(MapArc3 const &arc, Color stroke, float stroke_width) noexcept {
    ArcGeom g{};
    if (!arc_from_three_points(arc, g)) {
        return;
    }
    int const segments = 36;
    float const start = g.a0;
    float const span = g.ccw ? norm_angle(g.a1 - g.a0) : norm_angle(g.a0 - g.a1);
    for (int i = 0; i < segments; ++i) {
        float const t0 = static_cast<float>(i) / static_cast<float>(segments);
        float const t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
        float const a0 = g.ccw ? start + t0 * span : start - t0 * span;
        float const a1 = g.ccw ? start + t1 * span : start - t1 * span;
        draw_ground_line_marking(g.cx + g.r * std::cos(a0), g.cy + g.r * std::sin(a0),
                                 g.cx + g.r * std::cos(a1), g.cy + g.r * std::sin(a1),
                                 stroke_width, stroke);
    }
}

[[nodiscard]] bool shape_is_board_outline(MapShape const &shape,
                                          BoardOutline const &board) noexcept {
    if (shape.kind != MapShapeKind::Rect || !shape.stroke.has_value()) {
        return false;
    }
    if (shape.corner_radius <= board.corner_radius * 0.85f) {
        return false;
    }
    return std::fabs(shape.rect.x - board.rect.x) < 2.f &&
           std::fabs(shape.rect.y - board.rect.y) < 2.f &&
           std::fabs(shape.rect.w - board.rect.w) < 2.f &&
           std::fabs(shape.rect.h - board.rect.h) < 2.f;
}

void draw_map_shape_stroke_3d(MapShape const &shape,
                              std::optional<BoardOutline> const &board) noexcept {
    if (!shape.stroke.has_value()) {
        return;
    }
    Color const stroke_c = parse_map_css_color(*shape.stroke);
    if (stroke_c.a == 0) {
        return;
    }
    float const width = std::max(4.f, shape.stroke_width);

    switch (shape.kind) {
        case MapShapeKind::Rect:
            if (board.has_value() && shape_is_board_outline(shape, *board)) {
                break;
            }
            draw_round_rect_stroke_marking(shape.rect, shape.corner_radius, stroke_c, width);
            break;
        case MapShapeKind::Circle:
            if (shape.radius <= 40.f) {
                draw_ground_dot(shape.x, shape.y, shape.radius, stroke_c);
            } else {
                draw_ground_ring_marking(shape.x, shape.y, shape.radius, width, stroke_c);
            }
            break;
        case MapShapeKind::Line:
            draw_ground_line_marking(shape.line_p1.x, shape.line_p1.y, shape.line_p2.x,
                                     shape.line_p2.y, width, stroke_c);
            break;
        case MapShapeKind::Arc:
            draw_map_arc_stroke_marking(shape.arc, stroke_c, width);
            break;
    }
}

struct ShapeLayer {
    std::size_t index{};
    int z{};
};

[[nodiscard]] RectF background_dest(MapDefinition const &map) noexcept {
    if (map.background.has_value()) {
        RectF const &dest = map.background->dest;
        if (dest.w > 0.f && dest.h > 0.f) {
            return dest;
        }
    }
    if (map.player_bounds.w > 0.f && map.player_bounds.h > 0.f) {
        return map.player_bounds;
    }
    return map.camera_bounds;
}

[[nodiscard]] Color ice_fill_color(MapDefinition const &map) noexcept {
    Color fill{static_cast<unsigned char>(kDefaultIceColor[0]),
               static_cast<unsigned char>(kDefaultIceColor[1]),
               static_cast<unsigned char>(kDefaultIceColor[2]),
               static_cast<unsigned char>(kDefaultIceColor[3])};
    if (map.background.has_value() && map.background->color.has_value()) {
        fill = parse_map_css_color(*map.background->color);
    }
    return fill;
}

[[nodiscard]] RectF surround_outer_bounds(MapDefinition const &map) noexcept {
    if (map.camera_bounds.w > 0.f && map.camera_bounds.h > 0.f) {
        return map.camera_bounds;
    }
    return background_dest(map);
}

void draw_map_surround_only_3d(MapDefinition const &map) noexcept {
    RectF const outer = surround_outer_bounds(map);
    if (outer.w <= 0.f || outer.h <= 0.f || map.player_bounds.w <= 0.f || map.player_bounds.h <= 0.f) {
        return;
    }

    Color const fill = ice_fill_color(map);
    if (fill.a == 0) {
        return;
    }

    // Full outer floor; ice (step 1) sits above at kIceSurfaceY. Solo surround is visibly obvious.
    Color const surround = ColorBrightness(fill, -0.20f);
    draw_ice_surface_rect(outer, surround, kSurroundY);
}

void draw_map_ice_only_3d(MapDefinition const &map) noexcept {
    RectF const dest = background_dest(map);
    if (dest.w <= 0.f || dest.h <= 0.f) {
        return;
    }

    Color const fill = ice_fill_color(map);
    if (fill.a == 0) {
        return;
    }

    if (map.player_bounds.w > 0.f && map.player_bounds.h > 0.f) {
        draw_ice_surface_rect(map.player_bounds, fill, kIceSurfaceY);
        return;
    }

    draw_ice_surface_rect(dest, fill, kIceSurfaceY);
}

void draw_map_background_3d(MapDefinition const &map) noexcept {
    RectF const dest = background_dest(map);
    if (dest.w <= 0.f || dest.h <= 0.f) {
        return;
    }

    Color const fill = ice_fill_color(map);
    if (fill.a == 0) {
        return;
    }

    if (map.player_bounds.w > 0.f && map.player_bounds.h > 0.f) {
        draw_map_surround_only_3d(map);
        draw_map_ice_only_3d(map);
        return;
    }

    draw_ice_surface_rect(dest, fill, kIceSurfaceY);
}

void draw_map_stadium_backdrop_3d(MapDefinition const &map) noexcept {
    RectF bounds = background_dest(map);
    if (bounds.w <= 0.f || bounds.h <= 0.f) {
        bounds = map.camera_bounds;
    }
    if (bounds.w <= 0.f || bounds.h <= 0.f) {
        return;
    }

    float const pad_x = 220.f;
    float const pad_z = 260.f;
    float const x0 = bounds.x - pad_x;
    float const z0 = bounds.y - pad_z;
    float const x1 = bounds.x + bounds.w + pad_x;
    float const z1 = bounds.y + bounds.h + pad_z;

    Color const conc{118, 126, 138, 255};
    Color const stands{88, 96, 112, 255};
    Color const sky{118, 142, 172, 255};

    RectF const &inner = background_dest(map);
    if (inner.w > 0.f && inner.h > 0.f) {
        // Apron ring outside the playfield rect only — avoids z-fighting with surround/ice.
        if (z0 < inner.y - 1.f) {
            draw_ice_surface_rect(RectF{x0, z0, x1 - x0, inner.y - z0}, conc, kStadiumApronY);
        }
        float const inner_bottom = inner.y + inner.h;
        if (z1 > inner_bottom + 1.f) {
            draw_ice_surface_rect(RectF{x0, inner_bottom, x1 - x0, z1 - inner_bottom}, conc,
                                  kStadiumApronY);
        }
        if (x0 < inner.x - 1.f) {
            draw_ice_surface_rect(RectF{x0, inner.y, inner.x - x0, inner.h}, conc, kStadiumApronY);
        }
        float const inner_right = inner.x + inner.w;
        if (x1 > inner_right + 1.f) {
            draw_ice_surface_rect(RectF{inner_right, inner.y, x1 - inner_right, inner.h}, conc,
                                  kStadiumApronY);
        }
    }

    auto stand_wall = [&](float const sx, float const sz0, float const sz1, float const height_y,
                          Color const color) {
        draw_axis_aligned_wall_slab(sx, (sz0 + sz1) * 0.5f, kStadiumWallThickness, sz1 - sz0,
                                    height_y, color);
    };

    auto end_wall = [&](float const sz, float const height_y, Color const color) {
        draw_axis_aligned_wall_slab((x0 + x1) * 0.5f, sz, x1 - x0, kStadiumWallThickness, height_y,
                                    color);
    };

    stand_wall(x0, z0, z1, 72.f, stands);
    stand_wall(x1, z0, z1, 72.f, stands);
    end_wall(z1, 96.f, sky);
    end_wall(z0, 72.f, stands);
}

}  // namespace

Color parse_map_css_color(std::string const &text) noexcept {
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

char const *playfield_build_step_name(PlayfieldBuildStep const step) noexcept {
    switch (step) {
        case PlayfieldBuildStep::Origin:
            return "0 Origin (spawn axes)";
        case PlayfieldBuildStep::IceSurface:
            return "1 Ice surface";
        case PlayfieldBuildStep::Surround:
            return "2 Surround floor (camera_bounds, under ice)";
        case PlayfieldBuildStep::StadiumBackdrop:
            return "3 Stadium backdrop";
        case PlayfieldBuildStep::BoardWalls:
            return "4 Board walls";
        case PlayfieldBuildStep::Goals:
            return "5 Goal frames";
        case PlayfieldBuildStep::RinkMarkings:
            return "6 Rink markings (3D)";
        case PlayfieldBuildStep::PlayerProxy:
            return "7 Player (team mesh)";
        case PlayfieldBuildStep::Puck:
            return "8 Puck";
        case PlayfieldBuildStep::OpponentSkater:
            return "9 Opponent skater";
        case PlayfieldBuildStep::GameplayFx:
            return "10 Waypoint + shield FX";
        case PlayfieldBuildStep::FullPlayingPath:
            return "11 Full playing draw path";
        case PlayfieldBuildStep::Count:
            break;
    }
    return "?";
}

void draw_playfield_build_step_3d(MapDefinition const &map, PlayfieldBuildStep const step) noexcept {
    switch (step) {
        case PlayfieldBuildStep::Origin:
            break;
        case PlayfieldBuildStep::IceSurface:
            draw_map_ice_only_3d(map);
            break;
        case PlayfieldBuildStep::Surround:
            draw_map_surround_only_3d(map);
            break;
        case PlayfieldBuildStep::StadiumBackdrop:
            draw_map_stadium_backdrop_3d(map);
            break;
        case PlayfieldBuildStep::BoardWalls: {
            std::optional<BoardOutline> const board = find_board_outline(map);
            if (board.has_value()) {
                rlDisableBackfaceCulling();
                draw_board_walls_3d(*board);
                rlEnableBackfaceCulling();
            }
            break;
        }
        case PlayfieldBuildStep::Goals:
            draw_map_goals_3d(map);
            break;
        case PlayfieldBuildStep::RinkMarkings:
            draw_map_rink_strokes_3d(map);
            break;
        case PlayfieldBuildStep::PlayerProxy:
        case PlayfieldBuildStep::Puck:
        case PlayfieldBuildStep::OpponentSkater:
        case PlayfieldBuildStep::GameplayFx:
        case PlayfieldBuildStep::FullPlayingPath:
        case PlayfieldBuildStep::Count:
            break;
    }
}

void draw_map_playfield_3d(MapDefinition const &map) noexcept {
    rlDisableBackfaceCulling();
    draw_map_stadium_backdrop_3d(map);
    draw_map_background_3d(map);

    std::optional<BoardOutline> const board = find_board_outline(map);
    if (board.has_value()) {
        draw_board_walls_3d(*board);
    }
    rlEnableBackfaceCulling();

    draw_map_goals_3d(map);
}

void draw_map_rink_strokes_3d(MapDefinition const &map) noexcept {
    std::optional<BoardOutline> const board = find_board_outline(map);

    std::vector<ShapeLayer> layers;
    layers.reserve(map.shapes.size());
    for (std::size_t i = 0; i < map.shapes.size(); ++i) {
        MapShape const &shape = map.shapes[i];
        int const z = shape.z.has_value() ? *shape.z : static_cast<int>(i);
        layers.push_back(ShapeLayer{i, z});
    }
    std::sort(layers.begin(), layers.end(),
              [](ShapeLayer const &a, ShapeLayer const &b) { return a.z < b.z; });

    rlDisableBackfaceCulling();
    for (ShapeLayer const &layer : layers) {
        draw_map_shape_stroke_3d(map.shapes[layer.index], board);
    }
    rlEnableBackfaceCulling();
}

}  // namespace zh::ui
