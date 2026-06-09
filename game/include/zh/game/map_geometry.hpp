#pragma once

// Map collider math: arcs, round-rect bounce, goal zone bounds and overlap.

#include "zh/game/map_definition.hpp"

#include <cmath>

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

struct GoalEnclosureLayout {
    float mouth_x{};
    float back_x{};
    float top_y{};
    float bot_y{};

    [[nodiscard]] float depth() const noexcept { return std::abs(mouth_x - back_x); }
    [[nodiscard]] float width() const noexcept { return bot_y - top_y; }
};

// west_goal opens toward +X; east goal opens toward -X.
[[nodiscard]] GoalEnclosureLayout goal_enclosure_layout(RectF const &zone,
                                                        bool west_goal) noexcept;

struct GoalEnclosureCorner {
    float x{};
    float y{};
};

// Four interior corners of the U-shaped goal shell (back×top, back×bottom, mouth×top, mouth×bottom).
void goal_enclosure_corners(GoalEnclosureLayout const &layout,
                            bool west_goal,
                            GoalEnclosureCorner out_corners[4]) noexcept;

// Three flat wall slabs + four corner cylinders (matches draw_goal_frame_3d).
void resolve_circle_against_goal_enclosure(float &px,
                                           float &py,
                                           float &vx,
                                           float &vy,
                                           float radius,
                                           float bounce,
                                           GoalEnclosureLayout const &layout,
                                           bool west_goal) noexcept;

void resolve_circle_against_goal_zone_enclosure(float &px,
                                                  float &py,
                                                  float &vx,
                                                  float &vy,
                                                  float radius,
                                                  float bounce,
                                                  MapGoalZone const &zone,
                                                  bool west_goal) noexcept;

struct CapsuleBoardOutline {
    RectF bounds{};
    float end_radius{};
    float flat_left_x{};
    float flat_right_x{};
    float min_y{};
    float max_y{};
    float mid_y{};
};

[[nodiscard]] CapsuleBoardOutline capsule_board_outline(RectF const &player_bounds) noexcept;

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
