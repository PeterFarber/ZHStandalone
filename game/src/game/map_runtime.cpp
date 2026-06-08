// Process-wide MapDefinition accessor: file load, bounds, spawns, camera clamp.

#include "zh/game/map_runtime.hpp"

#include "zh/game/map_io.hpp"

#include <algorithm>
#include <cmath>

namespace zh::game {

namespace {

MapRuntime g_map_runtime{};

}  // namespace

MapRuntime &map_runtime() noexcept {
    return g_map_runtime;
}

void MapRuntime::reset_to_baked_default() noexcept {
    def_ = make_baked_default_map();
    loaded_from_file_ = false;
}

bool MapRuntime::load_from_file(char const *path) noexcept {
    MapDefinition loaded{};
    if (!load_map_file(path, loaded)) {
        return false;
    }
    def_ = std::move(loaded);
    loaded_from_file_ = true;
    return true;
}

// Playable clamp rect: explicit player_bounds, else first ice tile, else camera art bounds.
RectF MapRuntime::player_bounds() const noexcept {
    if (def_.player_bounds.w > 0.f && def_.player_bounds.h > 0.f) {
        return def_.player_bounds;
    }
    for (MapVisualPiece const &piece : def_.visuals) {
        if (piece.kind == MapVisualKind::IceTile) {
            return piece.dest;
        }
    }
    return def_.camera_bounds;
}

RectF MapRuntime::ice_bounds() const noexcept {
    return player_bounds();
}

float MapRuntime::min_x() const noexcept {
    RectF const b = ice_bounds();
    return b.x;
}

float MapRuntime::max_x() const noexcept {
    RectF const b = ice_bounds();
    return b.x + b.w;
}

float MapRuntime::min_y() const noexcept {
    RectF const b = ice_bounds();
    return b.y;
}

float MapRuntime::max_y() const noexcept {
    RectF const b = ice_bounds();
    return b.y + b.h;
}

// Keep Camera2D center inside camera_bounds; pin to art center when zoom shows full width/height.
void MapRuntime::clamp_camera_target(float &target_x,
                                     float &target_y,
                                     float const viewport_w,
                                     float const viewport_h,
                                     float const zoom) const noexcept {
    float const half_w = viewport_w * 0.5f / zoom;
    float const half_h = viewport_h * 0.5f / zoom;

    float const art_min_x = def_.camera_bounds.x;
    float const art_min_y = def_.camera_bounds.y;
    float const art_max_x = def_.camera_bounds.x + def_.camera_bounds.w;
    float const art_max_y = def_.camera_bounds.y + def_.camera_bounds.h;
    float const art_cx = (art_min_x + art_max_x) * 0.5f;
    float const art_cy = (art_min_y + art_max_y) * 0.5f;

    if (def_.camera_bounds.w <= half_w * 2.f) {
        target_x = art_cx;
    } else {
        target_x = std::clamp(target_x, art_min_x + half_w, art_max_x - half_w);
    }

    if (def_.camera_bounds.h <= half_h * 2.f) {
        target_y = art_cy;
    } else {
        target_y = std::clamp(target_y, art_min_y + half_h, art_max_y - half_h);
    }
}

// Roster spawn from map JSON when set; otherwise heuristic lanes along each end board.
MapPoint MapRuntime::player_spawn_or_default(bool const team_b,
                                             bool const is_goalie,
                                             std::size_t const skater_index) const noexcept {
    MapTeamSpawns const &team = team_b ? def_.spawns.team_b : def_.spawns.team_a;
    if (is_goalie && team.goalie) {
        return *team.goalie;
    }
    if (!is_goalie && skater_index < team.skaters.size() && team.skaters[skater_index]) {
        return *team.skaters[skater_index];
    }

    float const mid = mid_y();
    float const span = max_x() - min_x();
    float const inset = std::max(80.f, span * 0.08f);
    float const lane = std::max(56.f, span * 0.045f);
    float const x =
        team_b ? (max_x() - inset - static_cast<float>(skater_index) * lane * 0.35f)
               : (min_x() + inset + static_cast<float>(skater_index) * lane * 0.35f);
    float const y =
        is_goalie ? mid
                  : mid + (static_cast<int>(skater_index) - 1) * std::max(48.f, span * 0.04f);
    return {x, y};
}

}  // namespace zh::game
