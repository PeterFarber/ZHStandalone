// Baked default rink MapDefinition and capsule end-board collider builder.

#include "zh/game/map_definition.hpp"

#include <algorithm>
#include <string>

namespace zh::game {

namespace {

// Stadium capsule: flat top/bottom rects plus quarter-circle end caps (arc colliders).
void append_capsule_board_colliders_impl(std::vector<MapCollider> &colliders,
                                        float const min_x,
                                        float const max_x,
                                        float const min_y,
                                        float const max_y,
                                        float const wall) {
    float const r = (max_y - min_y) * 0.5f;
    float const cy = (min_y + max_y) * 0.5f;
    float const cx_l = min_x + r;
    float const cx_r = max_x - r;
    float const flat_w = std::max(0.f, cx_r - cx_l);

    colliders.push_back(MapCollider{
        .kind = MapColliderKind::Rect,
        .rect = RectF{cx_l, min_y - wall * 0.5f, flat_w, wall},
    });
    colliders.push_back(MapCollider{
        .kind = MapColliderKind::Rect,
        .rect = RectF{cx_l, max_y - wall * 0.5f, flat_w, wall},
    });
    colliders.push_back(MapCollider{
        .kind = MapColliderKind::Arc,
        .arc = MapArc3{MapPoint{cx_l, min_y}, MapPoint{min_x, cy}, MapPoint{cx_l, max_y}},
    });
    colliders.push_back(MapCollider{
        .kind = MapColliderKind::Arc,
        .arc = MapArc3{MapPoint{cx_r, min_y}, MapPoint{max_x, cy}, MapPoint{cx_r, max_y}},
    });

    // Quarter-circle junctions (capsule fillets between flat boards and end arcs).
    auto corner_capsule = [&](float const cx, float const cy) {
        colliders.push_back(MapCollider{
            .kind = MapColliderKind::Circle,
            .rect = RectF{cx, cy, 0.f, 0.f},
            .radius = wall * 0.55f,
            .tag = "board_corner_capsule",
        });
    };
    corner_capsule(cx_l, min_y);
    corner_capsule(cx_l, max_y);
    corner_capsule(cx_r, min_y);
    corner_capsule(cx_r, max_y);
}

}  // namespace

void append_capsule_board_colliders(std::vector<MapCollider> &colliders,
                                    float const min_x,
                                    float const max_x,
                                    float const min_y,
                                    float const max_y,
                                    float const wall_thickness) noexcept {
    append_capsule_board_colliders_impl(colliders, min_x, max_x, min_y, max_y, wall_thickness);
}

MapDefinition make_baked_default_map() noexcept {
    MapDefinition map{};
    map.version = kMapSchemaVersion;
    map.name = "default";
    map.world_per_tex = 1.75f;

    float const min_x = 162.f * map.world_per_tex;
    float const max_x = (162.f + 1212.f) * map.world_per_tex;
    float const min_y = 132.f * map.world_per_tex;
    float const max_y = (132.f + 760.f) * map.world_per_tex;
    float const mid = (min_y + max_y) * 0.5f;
    float const cx = (min_x + max_x) * 0.5f;

    float const wsl = 56.f * map.world_per_tex;
    float const wst = 60.f * map.world_per_tex;
    float const wsr = 57.f * map.world_per_tex;
    float const wsb = 47.f * map.world_per_tex;

    map.goal_west = MapGoalZone{};
    map.goal_west->kind = MapZoneShapeKind::Rect;
    map.goal_west->rect = RectF{min_x, mid - 46.f * map.world_per_tex, 42.f * map.world_per_tex,
                                92.f * map.world_per_tex};
    map.goal_east = MapGoalZone{};
    map.goal_east->kind = MapZoneShapeKind::Rect;
    map.goal_east->rect = RectF{max_x - 42.f * map.world_per_tex, mid - 46.f * map.world_per_tex,
                                  42.f * map.world_per_tex, 92.f * map.world_per_tex};

    map.player_bounds = RectF{min_x, min_y, max_x - min_x, max_y - min_y};
    map.camera_bounds = RectF{min_x - wsl, min_y - wst, (max_x - min_x) + wsl + wsr,
                              (max_y - min_y) + wst + wsb};

    map.background = MapBackground{};
    map.background->color = "#e8f0f8";
    map.background->dest = map.camera_bounds;

    map.spawns.puck_start = MapPoint{cx, mid};
    map.spawns.puck_faceoff = MapPoint{cx, mid};
    map.spawns.host = MapPoint{min_x + (max_x - min_x) * 0.35f, mid};
    for (std::size_t i = 0; i < map.spawns.slots.size(); ++i) {
        map.spawns.slots[i] =
            MapPoint{min_x + 110.f + static_cast<float>(i) * 76.f,
                     mid + (((i & 1U) != 0U) ? 68.f : -62.f)};
    }

    float const spawn_inset = std::max(110.f, (max_x - min_x) * 0.08f);
    float const lane_y[3] = {mid - 70.f, mid, mid + 70.f};
    map.spawns.team_a.goalie = MapPoint{min_x + spawn_inset * 0.55f, mid};
    map.spawns.team_b.goalie = MapPoint{max_x - spawn_inset * 0.55f, mid};
    for (std::size_t i = 0; i < 3; ++i) {
        map.spawns.team_a.skaters[i] =
            MapPoint{min_x + spawn_inset + static_cast<float>(i) * 52.f, lane_y[i]};
        map.spawns.team_b.skaters[i] =
            MapPoint{max_x - spawn_inset - static_cast<float>(i) * 52.f, lane_y[i]};
    }

    append_capsule_board_colliders_impl(map.colliders, min_x, max_x, min_y, max_y, 20.f);

    return map;
}

}  // namespace zh::game
