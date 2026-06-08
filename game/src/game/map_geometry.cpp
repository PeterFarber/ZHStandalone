// Map collider geometry: arcs, round-rect bounce, goal zones (rect or 3-point arc).

#include "zh/game/map_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace zh::game {

namespace {

float norm_angle(float a) noexcept {
    constexpr float kTau = 6.28318530718f;
    a = std::fmod(a, kTau);
    if (a < 0.f) {
        a += kTau;
    }
    return a;
}

bool angle_between_ccw(float a0, float a_mid, float a1) noexcept {
    float const d0 = norm_angle(a_mid - a0);
    float const d1 = norm_angle(a1 - a0);
    return d1 >= d0;
}

float clamp_to_arc(float angle, ArcGeom const &g) noexcept {
    float const a = norm_angle(angle);
    float const start = norm_angle(g.a0);
    float const end = norm_angle(g.a1);
    if (g.ccw) {
        float const rel = norm_angle(a - start);
        float const span = norm_angle(end - start);
        if (rel <= span) {
            return a;
        }
        float const ds = norm_angle(start - a);
        float const de = norm_angle(end - a);
        return ds <= de ? start : end;
    }
    float const rel = norm_angle(start - a);
    float const span = norm_angle(start - end);
    if (rel <= span) {
        return a;
    }
    float const ds = norm_angle(start - a);
    float const de = norm_angle(end - a);
    return ds <= de ? start : end;
}

void reflect_velocity(float &vx, float &vy, float nx, float ny, float bounce) noexcept {
    float const vdotn = vx * nx + vy * ny;
    if (vdotn >= 0.f) {
        return;
    }
    vx -= 2.f * vdotn * nx;
    vy -= 2.f * vdotn * ny;
    vx *= bounce;
    vy *= bounce;
}

}  // namespace

bool arc_from_three_points(MapArc3 const &arc, ArcGeom &out) noexcept {
    float const d = 2.f * (arc.p1.x * (arc.p2.y - arc.p3.y) + arc.p2.x * (arc.p3.y - arc.p1.y) +
                           arc.p3.x * (arc.p1.y - arc.p2.y));
    if (std::fabs(d) < 1e-4f) {
        return false;
    }

    float const p1sq = arc.p1.x * arc.p1.x + arc.p1.y * arc.p1.y;
    float const p2sq = arc.p2.x * arc.p2.x + arc.p2.y * arc.p2.y;
    float const p3sq = arc.p3.x * arc.p3.x + arc.p3.y * arc.p3.y;
    out.cx = (p1sq * (arc.p2.y - arc.p3.y) + p2sq * (arc.p3.y - arc.p1.y) +
              p3sq * (arc.p1.y - arc.p2.y)) /
             d;
    out.cy = (p1sq * (arc.p3.x - arc.p2.x) + p2sq * (arc.p1.x - arc.p3.x) +
              p3sq * (arc.p2.x - arc.p1.x)) /
             d;
    out.r = std::hypot(arc.p1.x - out.cx, arc.p1.y - out.cy);
    if (out.r < 1e-3f) {
        return false;
    }

    out.a0 = std::atan2(arc.p1.y - out.cy, arc.p1.x - out.cx);
    out.a1 = std::atan2(arc.p2.y - out.cy, arc.p2.x - out.cx);
    float const a3 = std::atan2(arc.p3.y - out.cy, arc.p3.x - out.cx);
    out.ccw = angle_between_ccw(out.a0, a3, out.a1);
    return true;
}

RectF goal_zone_bounds(MapGoalZone const &zone) noexcept {
    if (zone.kind == MapZoneShapeKind::Rect) {
        return zone.rect;
    }
    ArcGeom g{};
    if (!arc_from_three_points(zone.arc, g)) {
        return RectF{};
    }
    float min_x = std::min({zone.arc.p1.x, zone.arc.p2.x, zone.arc.p3.x});
    float max_x = std::max({zone.arc.p1.x, zone.arc.p2.x, zone.arc.p3.x});
    float min_y = std::min({zone.arc.p1.y, zone.arc.p2.y, zone.arc.p3.y});
    float max_y = std::max({zone.arc.p1.y, zone.arc.p2.y, zone.arc.p3.y});
    for (int i = 0; i <= 16; ++i) {
        float const t = static_cast<float>(i) / 16.f;
        float ang = g.ccw ? g.a0 + t * norm_angle(g.a1 - g.a0)
                          : g.a0 - t * norm_angle(g.a0 - g.a1);
        float const x = g.cx + g.r * std::cos(ang);
        float const y = g.cy + g.r * std::sin(ang);
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }
    return RectF{min_x, min_y, max_x - min_x, max_y - min_y};
}

// Goal scoring overlap: AABB for rect zones; arc closest-point + chord bulge heuristic for arcs.
bool puck_overlaps_goal_zone(float puck_cx,
                             float puck_cy,
                             float puck_radius,
                             MapGoalZone const &zone) noexcept {
    if (zone.kind == MapZoneShapeKind::Rect) {
        RectF const &r = zone.rect;
        float const closest_x = std::clamp(puck_cx, r.x, r.x + r.w);
        float const closest_y = std::clamp(puck_cy, r.y, r.y + r.h);
        float const dx = puck_cx - closest_x;
        float const dy = puck_cy - closest_y;
        return dx * dx + dy * dy <= puck_radius * puck_radius;
    }

    ArcGeom g{};
    if (!arc_from_three_points(zone.arc, g)) {
        return false;
    }

    float const ang = clamp_to_arc(std::atan2(puck_cy - g.cy, puck_cx - g.cx), g);
    float const closest_x = g.cx + g.r * std::cos(ang);
    float const closest_y = g.cy + g.r * std::sin(ang);
    float best_dx = puck_cx - closest_x;
    float best_dy = puck_cy - closest_y;
    float best_dist_sq = best_dx * best_dx + best_dy * best_dy;

    for (MapPoint const *p : {&zone.arc.p1, &zone.arc.p2}) {
        float const dx = puck_cx - p->x;
        float const dy = puck_cy - p->y;
        float const dist_sq = dx * dx + dy * dy;
        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_dx = dx;
            best_dy = dy;
        }
    }

    float const chord_mid_x = (zone.arc.p1.x + zone.arc.p2.x) * 0.5f;
    float const chord_mid_y = (zone.arc.p1.y + zone.arc.p2.y) * 0.5f;
    float const bulge_x = zone.arc.p3.x - chord_mid_x;
    float const bulge_y = zone.arc.p3.y - chord_mid_y;
    float const to_puck_x = puck_cx - chord_mid_x;
    float const to_puck_y = puck_cy - chord_mid_y;
    if (bulge_x * to_puck_x + bulge_y * to_puck_y > 0.f) {
        float const dist_center = std::hypot(puck_cx - g.cx, puck_cy - g.cy);
        if (dist_center + puck_radius >= g.r - 4.f && dist_center - puck_radius <= g.r + 40.f) {
            return true;
        }
    }

    return best_dist_sq <= puck_radius * puck_radius;
}

// Circle vs arc / round-rect / line colliders — push out and reflect velocity.
void resolve_circle_arc_bounce(float &px,
                               float &py,
                               float &vx,
                               float &vy,
                               float const radius,
                               MapArc3 const &arc,
                               float const bounce) noexcept {
    ArcGeom g{};
    if (!arc_from_three_points(arc, g)) {
        return;
    }

    float const ang = clamp_to_arc(std::atan2(py - g.cy, px - g.cx), g);
    float closest_x = g.cx + g.r * std::cos(ang);
    float closest_y = g.cy + g.r * std::sin(ang);
    float best_dx = px - closest_x;
    float best_dy = py - closest_y;
    float best_dist = std::hypot(best_dx, best_dy);

    for (MapPoint const *p : {&arc.p1, &arc.p2}) {
        float const dx = px - p->x;
        float const dy = py - p->y;
        float const dist = std::hypot(dx, dy);
        if (dist < best_dist) {
            best_dist = dist;
            best_dx = dx;
            best_dy = dy;
            closest_x = p->x;
            closest_y = p->y;
        }
    }

    if (best_dist >= radius || best_dist < 1e-4f) {
        return;
    }

    float nx = best_dx / best_dist;
    float ny = best_dy / best_dist;
    float const pen = radius - best_dist;
    px += nx * pen;
    py += ny * pen;
    reflect_velocity(vx, vy, nx, ny, bounce);
}

namespace {

void closest_point_on_round_rect(float px,
                                 float py,
                                 RectF const &rect,
                                 float r,
                                 float &out_x,
                                 float &out_y) noexcept {
    r = std::clamp(r, 0.f, std::min(rect.w, rect.h) * 0.5f);
    if (r <= 0.f) {
        out_x = std::clamp(px, rect.x, rect.x + rect.w);
        out_y = std::clamp(py, rect.y, rect.y + rect.h);
        return;
    }

    float const left = rect.x + r;
    float const right = rect.x + rect.w - r;
    float const top = rect.y + r;
    float const bottom = rect.y + rect.h - r;

    if (px >= left && px <= right && py >= top && py <= bottom) {
        float d_top = py - rect.y;
        float d_bottom = rect.y + rect.h - py;
        float d_left = px - rect.x;
        float d_right = rect.x + rect.w - px;
        float min_d = d_top;
        out_x = px;
        out_y = rect.y;
        if (d_bottom < min_d) {
            min_d = d_bottom;
            out_y = rect.y + rect.h;
        }
        if (d_left < min_d) {
            out_x = rect.x;
            out_y = py;
        }
        if (d_right < min_d) {
            out_x = rect.x + rect.w;
            out_y = py;
        }
        return;
    }

    if (px >= left && px <= right && py < top) {
        out_x = px;
        out_y = rect.y;
        return;
    }
    if (px >= left && px <= right && py > bottom) {
        out_x = px;
        out_y = rect.y + rect.h;
        return;
    }
    if (py >= top && py <= bottom && px < left) {
        out_x = rect.x;
        out_y = py;
        return;
    }
    if (py >= top && py <= bottom && px > right) {
        out_x = rect.x + rect.w;
        out_y = py;
        return;
    }

    float const corner_cx = px < left ? left : right;
    float const corner_cy = py < top ? top : bottom;
    float dx = px - corner_cx;
    float dy = py - corner_cy;
    float len = std::hypot(dx, dy);
    if (len < 1e-6f) {
        out_x = corner_cx + r;
        out_y = corner_cy;
        return;
    }
    out_x = corner_cx + dx / len * r;
    out_y = corner_cy + dy / len * r;
}

}  // namespace

void resolve_circle_round_rect_bounce(float &px,
                                      float &py,
                                      float &vx,
                                      float &vy,
                                      float const radius,
                                      RectF const &rect,
                                      float corner_radius,
                                      float const bounce) noexcept {
    float closest_x = 0.f;
    float closest_y = 0.f;
    closest_point_on_round_rect(px, py, rect, corner_radius, closest_x, closest_y);

    float dx = px - closest_x;
    float dy = py - closest_y;
    float dist = std::hypot(dx, dy);
    if (dist >= radius) {
        return;
    }

    float nx = 0.f;
    float ny = 0.f;
    float pen = 0.f;
    if (dist > 1e-6f) {
        nx = dx / dist;
        ny = dy / dist;
        pen = radius - dist;
    } else {
        float const cx = rect.x + rect.w * 0.5f;
        float const cy = rect.y + rect.h * 0.5f;
        nx = px - cx;
        ny = py - cy;
        float const len = std::hypot(nx, ny);
        if (len > 1e-6f) {
            nx /= len;
            ny /= len;
        } else {
            nx = 0.f;
            ny = -1.f;
        }
        pen = radius;
    }

    px += nx * pen;
    py += ny * pen;
    reflect_velocity(vx, vy, nx, ny, bounce);
}

void resolve_circle_line_bounce(float &px,
                                float &py,
                                float &vx,
                                float &vy,
                                float const radius,
                                MapPoint const p1,
                                MapPoint const p2,
                                float const bounce) noexcept {
    float const sx = p2.x - p1.x;
    float const sy = p2.y - p1.y;
    float const len_sq = sx * sx + sy * sy;
    if (len_sq < 1e-6f) {
        return;
    }

    float t = ((px - p1.x) * sx + (py - p1.y) * sy) / len_sq;
    t = std::clamp(t, 0.f, 1.f);
    float const cx = p1.x + t * sx;
    float const cy = p1.y + t * sy;

    float dx = px - cx;
    float dy = py - cy;
    float dist_sq = dx * dx + dy * dy;
    float const r_sq = radius * radius;
    if (dist_sq >= r_sq) {
        return;
    }

    float nx = 0.f;
    float ny = 0.f;
    float pen = 0.f;
    if (dist_sq > 1e-6f) {
        float const dist = std::sqrt(dist_sq);
        nx = dx / dist;
        ny = dy / dist;
        pen = radius - dist;
    } else {
        float const len = std::sqrt(len_sq);
        nx = -sy / len;
        ny = sx / len;
        pen = radius;
    }

    px += nx * pen;
    py += ny * pen;
    reflect_velocity(vx, vy, nx, ny, bounce);
}

}  // namespace zh::game
