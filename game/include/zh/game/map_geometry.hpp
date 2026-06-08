#pragma once

// Map collider math: arcs, round-rect bounce, goal zone bounds and overlap.

#include "zh/game/map_definition.hpp"

namespace zh::game {

struct ArcGeom {
    float cx{};
    float cy{};
    float r{};
    float a0{};
    float a1{};
    bool ccw{};  // sweep direction for clamp_to_arc
};

[[nodiscard]] bool arc_from_three_points(MapArc3 const &arc, ArcGeom &out) noexcept;
[[nodiscard]] RectF goal_zone_bounds(MapGoalZone const &zone) noexcept;
[[nodiscard]] bool puck_overlaps_goal_zone(float puck_cx,
                                           float puck_cy,
                                           float puck_radius,
                                           MapGoalZone const &zone) noexcept;
void resolve_circle_arc_bounce(float &px,
                               float &py,
                               float &vx,
                               float &vy,
                               float radius,
                               MapArc3 const &arc,
                               float bounce) noexcept;
void resolve_circle_round_rect_bounce(float &px,
                                      float &py,
                                      float &vx,
                                      float &vy,
                                      float radius,
                                      RectF const &rect,
                                      float corner_radius,
                                      float bounce) noexcept;
void resolve_circle_line_bounce(float &px,
                                float &py,
                                float &vx,
                                float &vy,
                                float radius,
                                MapPoint p1,
                                MapPoint p2,
                                float bounce) noexcept;

}  // namespace zh::game
