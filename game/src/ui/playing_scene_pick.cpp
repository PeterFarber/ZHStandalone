#include "zh/ui/playing_scene_pick.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/map_definition.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/gfx/vulkan/vk_matrix.hpp"
#include "zh/ui/world_space_3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <vector>

namespace zh::ui {

namespace {

using namespace zh::game;

inline constexpr float kPuckDrawHeight = 3.5f;
inline constexpr float kBoardWallThickness = 10.f;
inline constexpr float kWallHeight = 44.f;
inline constexpr float kWallBaseY = 0.15f;
inline constexpr float kWallCapY = kWallHeight + 0.8f;
inline constexpr float kIceSlabThickness = 8.f;

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

void collect_round_rect_points(RectF const &rect,
                              float corner_radius,
                              int arc_segments,
                              std::vector<Vector2> &out) {
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
    return std::nullopt;
}

struct Aabb {
    Vector3 min{};
    Vector3 max{};
};

[[nodiscard]] Aabb aabb_from_points(Vector3 const *pts, std::size_t count) noexcept {
    Aabb box{pts[0], pts[0]};
    for (std::size_t i = 1; i < count; ++i) {
        box.min.x = std::min(box.min.x, pts[i].x);
        box.min.y = std::min(box.min.y, pts[i].y);
        box.min.z = std::min(box.min.z, pts[i].z);
        box.max.x = std::max(box.max.x, pts[i].x);
        box.max.y = std::max(box.max.y, pts[i].y);
        box.max.z = std::max(box.max.z, pts[i].z);
    }
    return box;
}

[[nodiscard]] Aabb wall_segment_aabb(float ax,
                                     float ay,
                                     float bx,
                                     float by,
                                     float y_bottom,
                                     float y_top,
                                     float thickness,
                                     float ref_x,
                                     float ref_y) noexcept {
    float const dx = bx - ax;
    float const dy = by - ay;
    float const len = std::hypot(dx, dy);
    if (len <= 1e-3f || y_top <= y_bottom + 1e-3f) {
        return Aabb{};
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

    Vector3 corners[8] = {
        sim_point(ax - ox, ay - oy, y_bottom),
        sim_point(bx - ox, by - oy, y_bottom),
        sim_point(bx + ox, by + oy, y_bottom),
        sim_point(ax + ox, ay + oy, y_bottom),
        sim_point(ax - ox, ay - oy, y_top),
        sim_point(bx - ox, by - oy, y_top),
        sim_point(bx + ox, by + oy, y_top),
        sim_point(ax + ox, ay + oy, y_top),
    };
    return aabb_from_points(corners, 8);
}

[[nodiscard]] bool ray_hit_aabb(Vector3 const &origin,
                              Vector3 const &dir,
                              Aabb const &box,
                              float &t_out,
                              Vector3 &point_out) noexcept {
    float tmin = 0.f;
    float tmax = 1e30f;

    float const origins[3] = {origin.x, origin.y, origin.z};
    float const dirs[3] = {dir.x, dir.y, dir.z};
    float const mins[3] = {box.min.x, box.min.y, box.min.z};
    float const maxs[3] = {box.max.x, box.max.y, box.max.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(dirs[axis]) < 1e-8f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) {
                return false;
            }
            continue;
        }
        float t1 = (mins[axis] - origins[axis]) / dirs[axis];
        float t2 = (maxs[axis] - origins[axis]) / dirs[axis];
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) {
            return false;
        }
    }

    if (tmax < 0.f) {
        return false;
    }
    t_out = tmin >= 0.f ? tmin : tmax;
    point_out = Vector3{origin.x + dir.x * t_out, origin.y + dir.y * t_out,
                        origin.z + dir.z * t_out};
    return true;
}

[[nodiscard]] bool ray_hit_vertical_cylinder(Vector3 const &origin,
                                             Vector3 const &dir,
                                             Vector3 const &center,
                                             float radius,
                                             float y_min,
                                             float y_max,
                                             float &t_out,
                                             Vector3 &point_out) noexcept {
    if (radius <= 0.f) {
        return false;
    }

    float const ox = origin.x - center.x;
    float const oz = origin.z - center.z;
    float const dx = dir.x;
    float const dz = dir.z;

    float const a = dx * dx + dz * dz;
    float const b = 2.f * (ox * dx + oz * dz);
    float const c = ox * ox + oz * oz - radius * radius;

    auto try_t = [&](float t) -> bool {
        if (t < 1e-4f) {
            return false;
        }
        float const y = origin.y + t * dir.y;
        if (y < y_min - 1e-3f || y > y_max + 1e-3f) {
            return false;
        }
        t_out = t;
        point_out = Vector3{origin.x + t * dir.x, y, origin.z + t * dir.z};
        return true;
    };

    if (a < 1e-8f) {
        if (ox * ox + oz * oz > radius * radius) {
            return false;
        }
        if (std::fabs(dir.y) < 1e-8f) {
            return false;
        }
        for (float y_target : {y_min, y_max}) {
            float const t = (y_target - origin.y) / dir.y;
            if (try_t(t)) {
                return true;
            }
        }
        return false;
    }

    float const disc = b * b - 4.f * a * c;
    if (disc < 0.f) {
        return false;
    }
    float const sq = std::sqrt(disc);
    float const t0 = (-b - sq) / (2.f * a);
    float const t1 = (-b + sq) / (2.f * a);
    if (try_t(t0)) {
        return true;
    }
    return try_t(t1);
}

void push_hit(PlayingPickResult &result, PlayingPickHit hit) noexcept {
    if (result.hit_count >= PlayingPickResult::kMaxHits) {
        return;
    }
    result.hits[result.hit_count++] = hit;
}

void try_cylinder_hit(PlayingPickResult &result,
                      Vector3 const &origin,
                      Vector3 const &dir,
                      Vector3 center,
                      float radius,
                      float height,
                      PlayingPickKind kind,
                      char const *label,
                      int slot_index = -1) noexcept {
    float const y_min = center.y - height * 0.5f;
    float const y_max = center.y + height * 0.5f;
    float t = 0.f;
    Vector3 point{};
    if (!ray_hit_vertical_cylinder(origin, dir, center, radius, y_min, y_max, t, point)) {
        return;
    }
    PlayingPickHit hit{};
    hit.kind = kind;
    hit.distance = t;
    hit.point = point;
    hit.sim = world3_to_sim(point);
    hit.slot_index = slot_index;
    std::snprintf(hit.label, sizeof(hit.label), "%s", label);
    push_hit(result, hit);
}

void try_aabb_hit(PlayingPickResult &result,
                  Vector3 const &origin,
                  Vector3 const &dir,
                  Aabb const &box,
                  PlayingPickKind kind,
                  char const *label) noexcept {
    float t = 0.f;
    Vector3 point{};
    if (!ray_hit_aabb(origin, dir, box, t, point)) {
        return;
    }
    PlayingPickHit hit{};
    hit.kind = kind;
    hit.distance = t;
    hit.point = point;
    hit.sim = world3_to_sim(point);
    std::snprintf(hit.label, sizeof(hit.label), "%s", label);
    push_hit(result, hit);
}

void pick_board_walls(PlayingPickResult &result,
                      Vector3 const &origin,
                      Vector3 const &dir,
                      BoardOutline const &board) noexcept {
    std::vector<Vector2> pts;
    collect_round_rect_points(board.rect, board.corner_radius, 12, pts);
    if (pts.size() < 2) {
        return;
    }

    float const ref_x = board.rect.x + board.rect.w * 0.5f;
    float const ref_y = board.rect.y + board.rect.h * 0.5f;
    float const kick_top = kWallBaseY + 3.5f;
    float const cap_thickness = kBoardWallThickness * 0.65f;

    for (std::size_t i = 0; i < pts.size(); ++i) {
        Vector2 const &a = pts[i];
        Vector2 const &b = pts[(i + 1) % pts.size()];
        try_aabb_hit(result, origin, dir,
                     wall_segment_aabb(a.x, a.y, b.x, b.y, kWallBaseY, kick_top,
                                       kBoardWallThickness, ref_x, ref_y),
                     PlayingPickKind::BoardWall, "board kick");
        try_aabb_hit(result, origin, dir,
                     wall_segment_aabb(a.x, a.y, b.x, b.y, kick_top, kWallHeight,
                                       kBoardWallThickness, ref_x, ref_y),
                     PlayingPickKind::BoardWall, "board wall");
        try_aabb_hit(result, origin, dir,
                     wall_segment_aabb(a.x, a.y, b.x, b.y, kWallHeight, kWallCapY, cap_thickness,
                                       ref_x, ref_y),
                     PlayingPickKind::BoardWall, "board cap");
    }
}

}  // namespace

PlayingPickResult pick_playing_scene(Vector2 const screen,
                                     Camera3D const &cam,
                                     PlayingWorldDraw const &world) noexcept {
    PlayingPickResult result{};
    result.screen = screen;
    result.ray = GetScreenToWorldRay(screen, cam);

    Vector3 const origin = result.ray.position;
    Vector3 const dir = result.ray.direction;

    if (detail::screen_to_sim_ray(screen, cam, result.ice_plane_sim, kPlayingIceY)) {
        result.ice_plane_ok = true;
        Vector2 projected{};
        Vector3 const world_pt =
            sim_to_world3(result.ice_plane_sim.x, result.ice_plane_sim.y, kPlayingIceY);
        if (ProjectWorldToScreenForPick(world_pt, cam, projected)) {
            float const dx = screen.x - projected.x;
            float const dy = screen.y - projected.y;
            result.ice_plane_err_px = std::sqrt(dx * dx + dy * dy);
        }

        float t = 0.f;
        Vector3 point{};
        if (std::fabs(dir.y) > 1e-5f) {
            t = (kPlayingIceY - origin.y) / dir.y;
            if (t > 1e-4f) {
                point = Vector3{origin.x + dir.x * t, kPlayingIceY, origin.z + dir.z * t};
                PlayingPickHit hit{};
                hit.kind = PlayingPickKind::IcePlane;
                hit.distance = t;
                hit.point = point;
                hit.sim = result.ice_plane_sim;
                std::snprintf(hit.label, sizeof(hit.label), "ice plane");
                push_hit(result, hit);
            }
        }
    }

    RectF const ice = rink_ice_bounds();
    if (ice.w > 0.f && ice.h > 0.f) {
        float const cx = ice.x + ice.w * 0.5f;
        float const cz = ice.y + ice.h * 0.5f;
        float const cy = kPlayingIceY - kIceSlabThickness * 0.5f;
        Aabb const slab{
            Vector3{cx - ice.w * 0.5f, cy - kIceSlabThickness * 0.5f, cz - ice.h * 0.5f},
            Vector3{cx + ice.w * 0.5f, cy + kIceSlabThickness * 0.5f, cz + ice.h * 0.5f},
        };
        try_aabb_hit(result, origin, dir, slab, PlayingPickKind::IceSlab, "ice slab");
    }

    MapDefinition const &map = map_runtime().definition();
    if (std::optional<BoardOutline> const board = find_board_outline(map)) {
        pick_board_walls(result, origin, dir, *board);
    }

    if (world.active) {
        Vector3 const puck_center = sim_point(world.puck_x, world.puck_y, kPuckDrawHeight * 0.5f);
        try_cylinder_hit(result, origin, dir, puck_center, kPuckDrawRadius, kPuckDrawHeight,
                         PlayingPickKind::Puck, "puck");

        if (world.host.active) {
            float const radius = player_draw_radius(world.host.goalie) * 1.35f;
            float const height = (world.host.goalie ? kGoalieBodyHeight : kSkaterBodyHeight) * 1.2f;
            Vector3 const center = sim_point(world.host.x, world.host.y, height * 0.5f);
            try_cylinder_hit(result, origin, dir, center, radius, height, PlayingPickKind::SkaterHost,
                             world.host.is_local ? "local host skater" : "host skater");
            if (world.host.has_waypoint) {
                Vector3 const wp = sim_point(world.host.way_x, world.host.way_y, 0.6f);
                try_cylinder_hit(result, origin, dir, wp, 7.f, 1.2f, PlayingPickKind::WaypointHost,
                                 "host waypoint");
            }
            if (world.host.shield_frac > 0.f) {
                Vector3 const shield = sim_point(world.host.x, world.host.y, 2.f);
                try_cylinder_hit(result, origin, dir, shield, kGoalieShieldRadius, 4.f,
                                 PlayingPickKind::ShieldHost, "host shield");
            }
        }

        for (std::size_t i = 0; i < world.slots.size(); ++i) {
            PlayingEntityDraw const &slot = world.slots[i];
            if (!slot.active) {
                continue;
            }
            float const radius = player_draw_radius(slot.goalie) * 1.35f;
            float const height = (slot.goalie ? kGoalieBodyHeight : kSkaterBodyHeight) * 1.2f;
            Vector3 const center = sim_point(slot.x, slot.y, height * 0.5f);
            char label[40];
            std::snprintf(label, sizeof(label), "slot %zu skater", i + 1);
            try_cylinder_hit(result, origin, dir, center, radius, height, PlayingPickKind::SkaterSlot,
                             label, static_cast<int>(i));
            if (slot.has_waypoint) {
                Vector3 const wp = sim_point(slot.way_x, slot.way_y, 0.6f);
                std::snprintf(label, sizeof(label), "slot %zu waypoint", i + 1);
                try_cylinder_hit(result, origin, dir, wp, 7.f, 1.2f, PlayingPickKind::WaypointSlot,
                                 label, static_cast<int>(i));
            }
            if (slot.shield_frac > 0.f) {
                Vector3 const shield = sim_point(slot.x, slot.y, 2.f);
                std::snprintf(label, sizeof(label), "slot %zu shield", i + 1);
                try_cylinder_hit(result, origin, dir, shield, kGoalieShieldRadius, 4.f,
                                 PlayingPickKind::ShieldSlot, label, static_cast<int>(i));
            }
        }
    }

    if (result.hit_count > 1) {
        std::sort(result.hits, result.hits + result.hit_count,
                  [](PlayingPickHit const &a, PlayingPickHit const &b) {
                      return a.distance < b.distance;
                  });
    }
    if (result.hit_count > 0) {
        result.closest = result.hits[0];
    }
    return result;
}

void log_playing_pick_to_stderr(char const *button_tag,
                                PlayingPickResult const &pick,
                                Vector2 screen,
                                Vector2 ice_sim,
                                bool ice_ok,
                                float sent_sim_x,
                                float sent_sim_y) noexcept {
    std::fprintf(stderr,
                 "[zh][pick][%s] screen=(%.0f,%.0f) ray_o=(%.1f,%.1f,%.1f) ray_d=(%.3f,%.3f,%.3f)\n",
                 button_tag != nullptr ? button_tag : "?", screen.x, screen.y, pick.ray.position.x,
                 pick.ray.position.y, pick.ray.position.z, pick.ray.direction.x,
                 pick.ray.direction.y, pick.ray.direction.z);
    std::fprintf(stderr,
                 "  ice_unproject ok=%d sim=(%.1f,%.1f) err_px=%.1f sent=(%.1f,%.1f) hits=%d\n",
                 ice_ok ? 1 : 0, ice_sim.x, ice_sim.y, pick.ice_plane_err_px, sent_sim_x,
                 sent_sim_y, pick.hit_count);

    for (int i = 0; i < pick.hit_count; ++i) {
        PlayingPickHit const &h = pick.hits[i];
        std::fprintf(stderr,
                     "  hit[%d] %s dist=%.1f sim=(%.1f,%.1f) world=(%.1f,%.1f,%.1f)%s\n", i,
                     h.label, h.distance, h.sim.x, h.sim.y, h.point.x, h.point.y, h.point.z,
                     h.slot_index >= 0 ? " slot" : "");
    }

    if (pick.closest.kind != PlayingPickKind::None) {
        std::fprintf(stderr, "  closest: %s dist=%.1f\n", pick.closest.label,
                     pick.closest.distance);
    } else {
        std::fprintf(stderr, "  closest: none\n");
    }
}

namespace {

using namespace zh::game;

[[nodiscard]] bool project_pick(Camera3D const &cam, Vector3 const world, Vector2 &out) noexcept {
    return ProjectWorldToScreenForPick(world, cam, out);
}

[[nodiscard]] bool project_pick_visible(Camera3D const &cam, Vector3 const world,
                                        Vector2 &out) noexcept {
    int const width = GetScreenWidth();
    int const height = GetScreenHeight();
    if (width <= 0 || height <= 0) {
        return false;
    }

    zh::gfx::vk::Mat4 const clip_from_world = zh::gfx::vk::mat4_proj_view(cam, width, height);
    zh::gfx::vk::ClipHomogeneous const clip =
        zh::gfx::vk::mat4_transform_homogeneous(clip_from_world, world);
    if (clip.w <= 1e-4f) {
        return false;
    }

    float const inv_w = 1.f / clip.w;
    float const ndc_x = clip.x * inv_w;
    float const ndc_y = clip.y * inv_w;
    out.x = (ndc_x + 1.f) * 0.5f * static_cast<float>(width);
    out.y = (ndc_y + 1.f) * 0.5f * static_cast<float>(height);
    float const margin = 96.f;
    return out.x >= -margin && out.y >= -margin &&
           out.x <= static_cast<float>(width) + margin &&
           out.y <= static_cast<float>(height) + margin;
}

void draw_screen_cross(Vector2 center, float radius, float thick, Color color) noexcept {
    DrawLineEx({center.x - radius, center.y}, {center.x + radius, center.y}, thick, color);
    DrawLineEx({center.x, center.y - radius}, {center.x, center.y + radius}, thick, color);
}

void draw_screen_marker(Vector2 center, float radius, Color fill, Color outline) noexcept {
    DrawCircleV(center, radius + 1.5f, outline);
    DrawCircleV(center, radius, fill);
}

void draw_ray_on_screen(Camera3D const &cam,
                        Ray const &ray,
                        float t_end,
                        Color color) noexcept {
    if (t_end <= 1e-3f) {
        return;
    }

    Vector3 const dir = ray.direction;
    Vector2 prev{};
    bool have_prev = false;
    int const steps = 24;
    for (int i = 1; i <= steps; ++i) {
        float const t = t_end * static_cast<float>(i) / static_cast<float>(steps);
        Vector3 const world =
            Vector3{ray.position.x + dir.x * t, ray.position.y + dir.y * t,
                    ray.position.z + dir.z * t};
        Vector2 screen{};
        if (!project_pick_visible(cam, world, screen)) {
            have_prev = false;
            continue;
        }
        if (have_prev) {
            DrawLineEx(prev, screen, 2.f, color);
        }
        prev = screen;
        have_prev = true;
    }
}

void draw_sim_marker_on_screen(Camera3D const &cam,
                               float sim_x,
                               float sim_y,
                               float y,
                               float radius,
                               Color fill,
                               Color outline) noexcept {
    Vector2 screen{};
    if (project_pick(cam, sim_to_world3(sim_x, sim_y, y), screen)) {
        draw_screen_marker(screen, radius, fill, outline);
    }
}

void draw_sim_line_on_screen(Camera3D const &cam,
                             float ax,
                             float ay,
                             float bx,
                             float by,
                             float y,
                             float thick,
                             Color color) noexcept {
    Vector2 a{};
    Vector2 b{};
    if (!project_pick(cam, sim_to_world3(ax, ay, y), a)) {
        return;
    }
    if (!project_pick(cam, sim_to_world3(bx, by, y), b)) {
        return;
    }
    DrawLineEx(a, b, thick, color);
}

void draw_ice_bounds_on_screen(Camera3D const &cam, Color color) noexcept {
    RectF const ice = rink_ice_bounds();
    if (ice.w <= 0.f || ice.h <= 0.f) {
        return;
    }
    float const x0 = ice.x;
    float const y0 = ice.y;
    float const x1 = ice.x + ice.w;
    float const y1 = ice.y + ice.h;
    draw_sim_line_on_screen(cam, x0, y0, x1, y0, kPlayingIceY, 1.5f, color);
    draw_sim_line_on_screen(cam, x1, y0, x1, y1, kPlayingIceY, 1.5f, color);
    draw_sim_line_on_screen(cam, x1, y1, x0, y1, kPlayingIceY, 1.5f, color);
    draw_sim_line_on_screen(cam, x0, y1, x0, y0, kPlayingIceY, 1.5f, color);
}

void draw_pick_markers_3d(float sim_x, float sim_y, Color color, float size) noexcept {
    DrawCube(sim_to_world3(sim_x, sim_y, kPlayingIceY + size * 0.5f), size, size * 0.35f, size,
             color);
}

}  // namespace

void draw_playing_pick_debug(Camera3D const &cam,
                             Vector2 const screen,
                             Vector2 const clamped_sim,
                             bool const pick_ok,
                             float const sent_sim_x,
                             float const sent_sim_y,
                             PlayingPickResult const &pick) noexcept {
    using namespace zh::game;

    Color const ray_col{255, 40, 255, 220};
    Color const err_col{255, 230, 40, 255};
    Color const bounds_col{255, 80, 80, 170};
    Color const raw_col{80, 255, 120, 255};
    Color const clamp_col{255, 70, 70, 255};
    Color const sent_col{80, 220, 255, 255};

    draw_screen_cross(screen, 12.f, 2.f, Color{20, 20, 20, 255});

    float ray_t_end = 900.f;
    if (pick.ice_plane_ok && std::fabs(pick.ray.direction.y) > 1e-5f) {
        ray_t_end = (kPlayingIceY - pick.ray.position.y) / pick.ray.direction.y;
        if (ray_t_end < 1.f) {
            ray_t_end = 900.f;
        }
    } else if (std::fabs(pick.ray.direction.y) > 1e-5f) {
        float const t_ice = (kPlayingIceY - pick.ray.position.y) / pick.ray.direction.y;
        if (t_ice > 1.f) {
            ray_t_end = t_ice * 1.05f;
        }
    }
    draw_ray_on_screen(cam, pick.ray, ray_t_end, ray_col);
    draw_ice_bounds_on_screen(cam, bounds_col);

    Vector2 projected_raw{};
    if (pick.ice_plane_ok) {
        if (project_pick(cam, sim_to_world3(pick.ice_plane_sim.x, pick.ice_plane_sim.y, kPlayingIceY),
                         projected_raw)) {
            DrawLineEx(screen, projected_raw, 2.5f, err_col);
            Color const hit_col =
                pick.ice_plane_err_px >= 0.f && pick.ice_plane_err_px <= 2.f ? raw_col : clamp_col;
            draw_screen_marker(projected_raw, 5.f, hit_col, Color{20, 20, 20, 255});
        }
        draw_sim_marker_on_screen(cam, pick.ice_plane_sim.x, pick.ice_plane_sim.y, kPlayingIceY + 0.5f,
                                  4.f, raw_col, Color{20, 20, 20, 255});
    }

    float const clamp_dx = clamped_sim.x - pick.ice_plane_sim.x;
    float const clamp_dy = clamped_sim.y - pick.ice_plane_sim.y;
    bool const sim_clamped = pick.ice_plane_ok && (clamp_dx * clamp_dx + clamp_dy * clamp_dy > 1.f);
    if (sim_clamped) {
        draw_sim_marker_on_screen(cam, clamped_sim.x, clamped_sim.y, kPlayingIceY + 0.8f, 4.5f,
                                    clamp_col, Color{20, 20, 20, 255});
        draw_sim_line_on_screen(cam, pick.ice_plane_sim.x, pick.ice_plane_sim.y, clamped_sim.x,
                                clamped_sim.y, kPlayingIceY + 0.2f, 2.f, clamp_col);
    }

    if (pick_ok) {
        draw_sim_marker_on_screen(cam, sent_sim_x, sent_sim_y, kPlayingIceY + 1.1f, 4.f, sent_col,
                                  Color{20, 20, 20, 255});
    }

    if (pick.closest.kind != PlayingPickKind::None &&
        pick.closest.kind != PlayingPickKind::IcePlane) {
        Vector2 closest_screen{};
        if (project_pick_visible(cam, pick.closest.point, closest_screen)) {
            DrawLineEx(screen, closest_screen, 1.5f, Color{255, 140, 40, 220});
            draw_screen_marker(closest_screen, 4.f, Color{255, 140, 40, 220},
                               Color{20, 20, 20, 255});
        }
    }

    BeginMode3D(cam);
    if (pick.ice_plane_ok) {
        draw_pick_markers_3d(pick.ice_plane_sim.x, pick.ice_plane_sim.y, raw_col, 8.f);
        if (sim_clamped) {
            draw_pick_markers_3d(clamped_sim.x, clamped_sim.y, clamp_col, 7.f);
        }
    }
    if (pick_ok) {
        draw_pick_markers_3d(sent_sim_x, sent_sim_y, sent_col, 6.f);
    }

    RectF const ice = rink_ice_bounds();
    if (ice.w > 0.f && ice.h > 0.f) {
        Color const corner_col{255, 60, 60, 180};
        float const marker = 8.f;
        auto corner = [&](float sx, float sy) {
            DrawCube(sim_to_world3(sx, sy, kPlayingIceY + 1.5f), marker, 2.f, marker, corner_col);
        };
        corner(ice.x, ice.y);
        corner(ice.x + ice.w, ice.y);
        corner(ice.x + ice.w, ice.y + ice.h);
        corner(ice.x, ice.y + ice.h);
    }
    EndMode3D();

    int y = 84;
    auto line = [&](char const *text) {
        DrawText(text, 12, y, 14, BLACK);
        y += 18;
    };

    char buf[360];
    std::snprintf(buf, sizeof(buf),
                  "F3 pick debug  mouse=(%.0f,%.0f)  ice_ok=%d  err=%.1fpx  hits=%d",
                  screen.x, screen.y, pick.ice_plane_ok ? 1 : 0, pick.ice_plane_err_px,
                  pick.hit_count);
    line(buf);
    std::snprintf(buf, sizeof(buf), "raw sim=(%.1f,%.1f)  clamped=(%.1f,%.1f)  sent=(%.1f,%.1f)",
                  pick.ice_plane_sim.x, pick.ice_plane_sim.y, clamped_sim.x, clamped_sim.y,
                  sent_sim_x, sent_sim_y);
    line(buf);
    std::snprintf(
        buf, sizeof(buf), "ray o=(%.0f,%.0f,%.0f)  d=(%.3f,%.3f,%.3f)  pick_ok=%d  clamped=%d",
        pick.ray.position.x, pick.ray.position.y, pick.ray.position.z, pick.ray.direction.x,
        pick.ray.direction.y, pick.ray.direction.z, pick_ok ? 1 : 0, sim_clamped ? 1 : 0);
    line(buf);
    std::snprintf(buf, sizeof(buf), "cam pos=(%.0f,%.0f,%.0f)  target=(%.0f,%.0f,%.0f)",
                  cam.position.x, cam.position.y, cam.position.z, cam.target.x, cam.target.y,
                  cam.target.z);
    line(buf);
    if (pick.closest.kind != PlayingPickKind::None) {
        std::snprintf(buf, sizeof(buf), "closest: %s  dist=%.0f  sim=(%.0f,%.0f)",
                      pick.closest.label, pick.closest.distance, pick.closest.sim.x,
                      pick.closest.sim.y);
        line(buf);
    } else {
        line("closest: none");
    }

    int const show_hits = std::min(pick.hit_count, 4);
    for (int i = 0; i < show_hits; ++i) {
        PlayingPickHit const &h = pick.hits[i];
        std::snprintf(buf, sizeof(buf), "  [%d] %s d=%.0f sim=(%.0f,%.0f)", i, h.label, h.distance,
                      h.sim.x, h.sim.y);
        line(buf);
    }

    line("magenta=ray  yellow=mouse->ice  green/red=ice hit  cyan=sent  orange=occluder");
}

}  // namespace zh::ui
