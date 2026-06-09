#pragma once

#include "zh/gfx/gfx_types.hpp"

namespace zh::gfx::vk {

// Perspective clip tuned for ~3000-unit rink maps (was 0.1/2000 — z-fighting + pop-in).
inline constexpr float kCameraNearPlane = 2.f;
inline constexpr float kCameraFarPlane = 12000.f;

// Column-major 4x4 (glm / VulkanTutorial std140 mat4). Index: m[col * 4 + row].
struct Mat4 {
    float m[16]{};
};

[[nodiscard]] Mat4 mat4_identity() noexcept;
[[nodiscard]] Mat4 mat4_multiply(Mat4 a, Mat4 b) noexcept;
[[nodiscard]] Mat4 mat4_invert(Mat4 m) noexcept;

[[nodiscard]] Mat4 mat4_translate(float x, float y, float z) noexcept;
[[nodiscard]] Mat4 mat4_rotate_z(float radians) noexcept;
[[nodiscard]] Mat4 mat4_scale(float x, float y, float z) noexcept;

// glm::lookAt — VulkanTutorial ch. 27 view matrix.
[[nodiscard]] Mat4 mat4_look_at(Vector3 eye, Vector3 target, Vector3 up) noexcept;

// glm::perspective + VulkanTutorial Y flip (ch. 22/27).
[[nodiscard]] Mat4 mat4_perspective_vulkan(float fovy_rad, float aspect, float near_plane,
                                             float far_plane) noexcept;

// Top-left screen ortho (y down) for 2D UI batch + standard viewport.
[[nodiscard]] Mat4 mat4_ortho_top_left(float width, float height) noexcept;

// Tutorial: gl_Position = proj * view * model * vec4(pos, 1); model = I here.
[[nodiscard]] Mat4 mat4_proj_view(Camera3D const &camera, int width, int height) noexcept;

[[nodiscard]] Mat4 mat4_camera_2d(Camera2D camera) noexcept;

[[nodiscard]] Vector3 mat4_transform_point(Mat4 m, Vector3 v) noexcept;

struct ClipHomogeneous {
    float x{};
    float y{};
    float z{};
    float w{1.f};
};

[[nodiscard]] ClipHomogeneous mat4_transform_homogeneous(Mat4 m, Vector3 v) noexcept;
[[nodiscard]] bool clip_homogeneous_visible(ClipHomogeneous clip) noexcept;

[[nodiscard]] Vector3 mat4_unproject_ndc(Vector3 ndc, Mat4 view, Mat4 proj) noexcept;

// Direct upload for GLSL `mat4 * vec4`.
void mat4_to_glsl(Mat4 const &m, float out[16]) noexcept;

}  // namespace zh::gfx::vk
