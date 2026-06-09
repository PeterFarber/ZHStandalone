#pragma once

// Screen/world projection helpers for the Vulkan gfx layer.

#include "zh/gfx/gfx_types.hpp"

#include <cmath>

namespace zh::gfx {

[[nodiscard]] inline Vector3 Vector3Add(Vector3 v1, Vector3 v2) noexcept {
    return Vector3{v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

[[nodiscard]] inline Vector3 Vector3Subtract(Vector3 v1, Vector3 v2) noexcept {
    return Vector3{v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

[[nodiscard]] inline Vector3 Vector3Scale(Vector3 v, float scalar) noexcept {
    return Vector3{v.x * scalar, v.y * scalar, v.z * scalar};
}

[[nodiscard]] inline float Vector2Distance(Vector2 v1, Vector2 v2) noexcept {
    float const dx = v2.x - v1.x;
    float const dy = v2.y - v1.y;
    return std::sqrt(dx * dx + dy * dy);
}

Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera);
[[nodiscard]] bool TryGetWorldToScreen(Vector3 position, Camera3D const &camera,
                                       Vector2 &out) noexcept;
// Picking / inverse projection — skips frustum cull so edge ice points still project.
[[nodiscard]] bool ProjectWorldToScreenForPick(Vector3 position, Camera3D const &camera,
                                               Vector2 &out) noexcept;
Vector2 GetWorldToScreen(Vector3 position, Camera3D camera);
Ray GetScreenToWorldRay(Vector2 screen, Camera3D camera);
// Inverse-project screen coords onto a horizontal plane (world Y = plane_y); out x/z sim coords.
[[nodiscard]] bool UnprojectScreenToPlaneY(Vector2 screen, Camera3D const &camera, float plane_y,
                                           Vector2 &out_sim) noexcept;

}  // namespace zh::gfx
