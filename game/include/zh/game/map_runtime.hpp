#pragma once

// Process-wide map state: definition, bounds, spawns, camera clamp.

#include "zh/game/map_definition.hpp"
#include "zh/game/map_geometry.hpp"
#include "zh/game/types.hpp"

namespace zh::game {

class MapRuntime {
  public:
    void reset_to_baked_default() noexcept;
    bool load_from_file(char const *path) noexcept;

    [[nodiscard]] MapDefinition const &definition() const noexcept { return def_; }
    [[nodiscard]] bool loaded_from_file() const noexcept { return loaded_from_file_; }

    [[nodiscard]] RectF ice_bounds() const noexcept;
    [[nodiscard]] float min_x() const noexcept;
    [[nodiscard]] float max_x() const noexcept;
    [[nodiscard]] float min_y() const noexcept;
    [[nodiscard]] float max_y() const noexcept;
    [[nodiscard]] float mid_y() const noexcept { return (min_y() + max_y()) * 0.5f; }
    [[nodiscard]] float center_x() const noexcept { return (min_x() + max_x()) * 0.5f; }

    [[nodiscard]] std::optional<MapGoalZone> const &west_goal() const noexcept { return def_.goal_west; }
    [[nodiscard]] std::optional<MapGoalZone> const &east_goal() const noexcept { return def_.goal_east; }
    [[nodiscard]] RectF left_goal_zone() const noexcept {
        return def_.goal_west ? goal_zone_bounds(*def_.goal_west) : RectF{};
    }
    [[nodiscard]] RectF right_goal_zone() const noexcept {
        return def_.goal_east ? goal_zone_bounds(*def_.goal_east) : RectF{};
    }
    [[nodiscard]] MapSpawns const &spawns() const noexcept { return def_.spawns; }
    [[nodiscard]] RectF player_bounds() const noexcept;
    [[nodiscard]] std::vector<MapCollider> const &colliders() const noexcept {
        return def_.colliders;
    }

    [[nodiscard]] MapPoint host_spawn_or_default() const noexcept {
        if (def_.spawns.host) {
            return *def_.spawns.host;
        }
        return player_spawn_or_default(false, false, 0);
    }

    [[nodiscard]] MapPoint player_spawn_or_default(bool team_b,
                                                   bool is_goalie,
                                                   std::size_t skater_index) const noexcept;

    [[nodiscard]] MapPoint slot_spawn_or_default(std::size_t index) const noexcept {
        if (index < def_.spawns.slots.size() && def_.spawns.slots[index]) {
            return *def_.spawns.slots[index];
        }
        return {center_x(), mid_y()};
    }

    [[nodiscard]] MapPoint puck_faceoff_or_default() const noexcept {
        if (def_.spawns.puck_faceoff) {
            return *def_.spawns.puck_faceoff;
        }
        if (def_.spawns.puck_start) {
            return *def_.spawns.puck_start;
        }
        return {center_x(), mid_y()};
    }

    void clamp_camera_target(float &target_x,
                             float &target_y,
                             float viewport_w,
                             float viewport_h,
                             float zoom) const noexcept;

  private:
    MapDefinition def_{make_baked_default_map()};
    bool loaded_from_file_{false};
};

// Process-wide map loaded at startup (baked default or maps/*.json via App).
[[nodiscard]] MapRuntime &map_runtime() noexcept;

}  // namespace zh::game
