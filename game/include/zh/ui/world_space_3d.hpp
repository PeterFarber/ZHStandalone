#pragma once

// Map authoritative 2D sim coordinates (x, y) onto the 3D playing plane (x, up, z).
// Same picking path as RenderTestScene::update_waypoint_input().

#include "zh/game/rink_layout.hpp"
#include "zh/gfx/gfx_compat.hpp"

#include <algorithm>
#include <cmath>

namespace zh::ui {

[[nodiscard]] inline Vector3 sim_to_world3(float sim_x, float sim_y, float height_y = 0.f) noexcept {
    return Vector3{sim_x, height_y, sim_y};
}

// Lit 3D playfield heights (map ice slab + painted markings).
inline constexpr float kPlayingIceY = 0.35f;
// Painted circles/lines sit above the ice surface to avoid depth fighting with the slab.
inline constexpr float kPlayingMarkY = kPlayingIceY + 0.20f;
inline constexpr float kPlayingWaypointDiscHeight = 1.2f;
inline constexpr float kPlayingWaypointY = kPlayingMarkY + kPlayingWaypointDiscHeight * 0.5f;
inline constexpr float kPlayingFxDiscHeight = 0.18f;
inline constexpr float kPlayingFxDiscY = kPlayingMarkY + kPlayingFxDiscHeight * 0.5f + 0.04f;

[[nodiscard]] inline Vector2 world3_to_sim(Vector3 const &p) noexcept {
    return Vector2{p.x, p.z};
}

namespace detail {

[[nodiscard]] inline bool screen_to_sim_ray(Vector2 screen,
                                            Camera3D const &cam,
                                            Vector2 &out_sim,
                                            float surface_y) noexcept {
    Ray const ray = GetScreenToWorldRay(screen, cam);
    if (std::fabs(ray.direction.y) < 1e-5f) {
        return false;
    }
    float const t = (surface_y - ray.position.y) / ray.direction.y;
    if (t < 0.f) {
        return false;
    }
    Vector3 const hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    out_sim = world3_to_sim(hit);
    return true;
}

[[nodiscard]] inline bool screen_to_sim_refine(Vector2 screen,
                                                 Camera3D const &cam,
                                                 Vector2 sim,
                                                 float surface_y,
                                                 Vector2 &out_sim) noexcept {
    Vector2 projected{};
    Vector3 const world = sim_to_world3(sim.x, sim.y, surface_y);
    if (!ProjectWorldToScreenForPick(world, cam, projected)) {
        return false;
    }

    float err_x = screen.x - projected.x;
    float err_y = screen.y - projected.y;
    if (err_x * err_x + err_y * err_y <= 1.f) {
        out_sim = sim;
        return true;
    }

    constexpr float eps = 3.f;
    Vector2 proj_dx{};
    Vector2 proj_dy{};
    if (!ProjectWorldToScreenForPick(sim_to_world3(sim.x + eps, sim.y, surface_y), cam, proj_dx)) {
        return false;
    }
    if (!ProjectWorldToScreenForPick(sim_to_world3(sim.x, sim.y + eps, surface_y), cam, proj_dy)) {
        return false;
    }

    float const j00 = (proj_dx.x - projected.x) / eps;
    float const j01 = (proj_dy.x - projected.x) / eps;
    float const j10 = (proj_dx.y - projected.y) / eps;
    float const j11 = (proj_dy.y - projected.y) / eps;
    float const det = j00 * j11 - j01 * j10;
    if (std::fabs(det) < 1e-10f) {
        return false;
    }

    float const inv_det = 1.f / det;
    Vector2 trial = sim;
    trial.x += (j11 * err_x - j01 * err_y) * inv_det;
    trial.y += (-j10 * err_x + j00 * err_y) * inv_det;

    if (!ProjectWorldToScreenForPick(sim_to_world3(trial.x, trial.y, surface_y), cam, projected)) {
        return false;
    }
    err_x = screen.x - projected.x;
    err_y = screen.y - projected.y;
    if (err_x * err_x + err_y * err_y <= 1.f) {
        out_sim = trial;
        return true;
    }
    return false;
}

}  // namespace detail

// Mouse → sim on the ice plane (RenderTestScene path: ray, optional refine, clamp to ice).
[[nodiscard]] inline bool screen_to_sim_ground(Vector2 screen,
                                               Camera3D const &cam,
                                               Vector2 &out_sim,
                                               float surface_y = kPlayingIceY) noexcept {
    using zh::game::RectF;
    using zh::game::rink_ice_bounds;

    Vector2 sim{};
    if (!detail::screen_to_sim_ray(screen, cam, sim, surface_y)) {
        return false;
    }

    Vector2 refined{};
    if (detail::screen_to_sim_refine(screen, cam, sim, surface_y, refined)) {
        sim = refined;
    }

    RectF const ice = rink_ice_bounds();
    out_sim.x = std::clamp(sim.x, ice.x, ice.x + ice.w);
    out_sim.y = std::clamp(sim.y, ice.y, ice.y + ice.h);
    return true;
}

}  // namespace zh::ui
