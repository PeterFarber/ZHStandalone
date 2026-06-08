#pragma once

// Map schema: visuals, colliders, spawns, goals (JSON-serializable MapDefinition).

#include "zh/game/types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zh::game {

inline constexpr int kMapSchemaVersion = 1;

struct MapBackground {
    std::optional<std::string> color{};
    std::optional<std::string> image{};
    RectF dest{};
    RectF src{};
    bool use_src{false};
    bool repeat{false};
};

enum class MapVisualKind : std::uint8_t {
    IceTile,
    WallFrame,
    CircleMark,
    GoalSprite,
};

enum class MapColliderKind : std::uint8_t {
    Rect,
    Circle,
    Arc,
    Line,
};

enum class MapShapeKind : std::uint8_t {
    Rect,
    Circle,
    Line,
    Arc,
};

enum class MapZoneShapeKind : std::uint8_t {
    Rect,
    Arc,
};

struct MapPoint {
    float x{};
    float y{};
};

struct MapArc3 {
    MapPoint p1{};
    MapPoint p2{};
    MapPoint p3{};
};

struct MapGoalZone {
    MapZoneShapeKind kind{MapZoneShapeKind::Rect};
    RectF rect{};
    MapArc3 arc{};
    std::optional<std::string> tag{};
};

struct MapVisualPiece {
    std::string id;
    MapVisualKind kind{MapVisualKind::IceTile};
    RectF dest{};
    RectF src{};
    bool use_src{false};
    bool flip_x{false};
    float slice_l{56.f};
    float slice_t{60.f};
    float slice_r{57.f};
    float slice_b{47.f};
    // Draw order among map visuals (lower = behind). When unset at load, array index is used.
    std::optional<int> z{};
    std::optional<std::string> tag{};
};

struct MapCollider {
    MapColliderKind kind{MapColliderKind::Rect};
    RectF rect{};
    float radius{};
    float corner_radius{};
    MapArc3 arc{};
    // Endpoints when **`kind == Line`** (also stored as JSON **`p1`** / **`p2`**).
    MapPoint line_p1{};
    MapPoint line_p2{};
    std::optional<std::string> tag{};
};

struct MapShape {
    MapShapeKind kind{MapShapeKind::Rect};
    RectF rect{};
    float x{};
    float y{};
    float radius{};
    float corner_radius{};
    MapArc3 arc{};
    MapPoint line_p1{};
    MapPoint line_p2{};
    std::optional<std::string> fill{};
    std::optional<std::string> stroke{};
    float stroke_width{2.f};
    std::optional<int> z{};
    std::optional<std::string> tag{};
};

struct MapTeamSpawns {
    std::optional<MapPoint> goalie{};
    std::array<std::optional<MapPoint>, 3> skaters{};
};

struct MapSpawns {
    std::optional<MapPoint> puck_start{};
    std::optional<MapPoint> puck_faceoff{};
    // Legacy listen-server host marker (optional); roster spawns use **`team_a`** / **`team_b`**.
    std::optional<MapPoint> host{};
    std::array<std::optional<MapPoint>, 4> slots{};
    MapTeamSpawns team_a{};
    MapTeamSpawns team_b{};
};

struct MapDefinition {
    int version{kMapSchemaVersion};
    std::string name{"default"};
    float world_per_tex{1.75f};
    // Playable skater/puck clamp region (world units). Prefer over ice-tile dest when set.
    RectF player_bounds{};
    // Camera pan limits (world units); usually includes decorative margin outside player_bounds.
    RectF camera_bounds{};
    std::optional<MapBackground> background{};
    std::vector<MapVisualPiece> visuals;
    // Editor color shapes (lines, circles, etc.) — drawn in-game when present.
    std::vector<MapShape> shapes;
    std::vector<MapCollider> colliders;
    std::optional<MapGoalZone> goal_west{};
    std::optional<MapGoalZone> goal_east{};
    MapSpawns spawns{};
};

[[nodiscard]] MapDefinition make_baked_default_map() noexcept;

void append_capsule_board_colliders(std::vector<MapCollider> &colliders,
                                    float min_x,
                                    float max_x,
                                    float min_y,
                                    float max_y,
                                    float wall_thickness = 20.f) noexcept;

}  // namespace zh::game
