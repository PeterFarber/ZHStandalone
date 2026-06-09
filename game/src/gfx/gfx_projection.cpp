#include "zh/gfx/gfx_projection.hpp"

#include "zh/gfx/gfx_constants.hpp"
#include "zh/gfx/gfx_compat.hpp"
#include "zh/gfx/vulkan/vk_matrix.hpp"

#include <cmath>

namespace {

using zh::gfx::Vector2;
using zh::gfx::Vector3;
using zh::gfx::Camera2D;
using zh::gfx::Camera3D;
using zh::gfx::vk::ClipHomogeneous;
using zh::gfx::vk::Mat4;

[[nodiscard]] Vector3 vector3_subtract(Vector3 a, Vector3 b) noexcept {
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] Vector3 vector3_normalize(Vector3 v) noexcept {
    float const len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 1e-6f) {
        return Vector3{0.f, 0.f, 0.f};
    }
    float const inv = 1.f / len;
    return Vector3{v.x * inv, v.y * inv, v.z * inv};
}

}  // namespace

Ray zh::gfx::GetScreenToWorldRay(Vector2 position, Camera3D camera) {
    using zh::gfx::vk::mat4_invert;
    using zh::gfx::vk::mat4_look_at;
    using zh::gfx::vk::mat4_multiply;
    using zh::gfx::vk::mat4_perspective_vulkan;
    using zh::gfx::vk::mat4_transform_point;
    using zh::gfx::vk::kCameraFarPlane;
    using zh::gfx::vk::kCameraNearPlane;

    int const width = GetScreenWidth();
    int const height = GetScreenHeight();
    Ray ray{};

    if (width <= 0 || height <= 0) {
        return ray;
    }

    // Same top-left screen → NDC mapping as ProjectWorldToScreenForPick (VulkanTutorial path).
    float const ndc_x = (2.f * position.x) / static_cast<float>(width) - 1.f;
    float const ndc_y = (2.f * position.y) / static_cast<float>(height) - 1.f;

    float const aspect =
        static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    Mat4 const view = mat4_look_at(camera.position, camera.target, camera.up);
    Mat4 const proj =
        mat4_perspective_vulkan(camera.fovy * DEG2RAD, aspect, kCameraNearPlane, kCameraFarPlane);
    Mat4 const inv = mat4_invert(mat4_multiply(proj, view));

    // glm::perspective OpenGL depth: unproject z = -1 (near) and z = +1 (far).
    Vector3 const near_point = mat4_transform_point(inv, Vector3{ndc_x, ndc_y, -1.f});
    Vector3 const far_point = mat4_transform_point(inv, Vector3{ndc_x, ndc_y, 1.f});
    ray.position = camera.position;
    ray.direction = vector3_normalize(vector3_subtract(far_point, near_point));
    return ray;
}

bool zh::gfx::UnprojectScreenToPlaneY(Vector2 screen, Camera3D const &camera, float const plane_y,
                                      Vector2 &out_sim) noexcept {
    using zh::gfx::vk::mat4_invert;
    using zh::gfx::vk::mat4_proj_view;
    using zh::gfx::vk::mat4_transform_point;

    int const width = GetScreenWidth();
    int const height = GetScreenHeight();
    if (width <= 0 || height <= 0) {
        return false;
    }

    float const ndc_x = (2.f * screen.x) / static_cast<float>(width) - 1.f;
    float const ndc_y = (2.f * screen.y) / static_cast<float>(height) - 1.f;
    Mat4 const inv = mat4_invert(mat4_proj_view(camera, width, height));

    auto world_y_at = [&](float clip_z) noexcept {
        Vector3 const p = mat4_transform_point(inv, Vector3{ndc_x, ndc_y, clip_z});
        return p.y;
    };

    float za = -1.f;
    float zb = 1.f;
    float ya = world_y_at(za);
    float yb = world_y_at(zb);
    if ((ya - plane_y) * (yb - plane_y) > 0.f) {
        return false;
    }

    for (int i = 0; i < 32; ++i) {
        float const zm = (za + zb) * 0.5f;
        float const ym = world_y_at(zm);
        if ((ya - plane_y) * (ym - plane_y) <= 0.f) {
            zb = zm;
            yb = ym;
        } else {
            za = zm;
            ya = ym;
        }
    }

    Vector3 const hit =
        mat4_transform_point(inv, Vector3{ndc_x, ndc_y, (za + zb) * 0.5f});
    out_sim = Vector2{hit.x, hit.z};
    return true;
}

Vector2 zh::gfx::GetScreenToWorld2D(Vector2 position, Camera2D camera) {
    Mat4 const inv = vk::mat4_invert(vk::mat4_camera_2d(camera));
    Vector3 const world = vk::mat4_transform_point(inv, Vector3{position.x, position.y, 0.f});
    return Vector2{world.x, world.y};
}

bool zh::gfx::ProjectWorldToScreenForPick(Vector3 position, Camera3D const &camera,
                                           Vector2 &out) noexcept {
    int const width = GetScreenWidth();
    int const height = GetScreenHeight();
    if (width <= 0 || height <= 0) {
        return false;
    }

    Mat4 const clip_from_world = vk::mat4_proj_view(camera, width, height);
    ClipHomogeneous const clip = vk::mat4_transform_homogeneous(clip_from_world, position);
    if (clip.w <= 1e-4f) {
        return false;
    }

    float const inv_w = 1.f / clip.w;
    float const ndc_x = clip.x * inv_w;
    float const ndc_y = clip.y * inv_w;
    out.x = (ndc_x + 1.f) * 0.5f * static_cast<float>(width);
    out.y = (ndc_y + 1.f) * 0.5f * static_cast<float>(height);
    return true;
}

bool zh::gfx::TryGetWorldToScreen(Vector3 position, Camera3D const &camera,
                                  Vector2 &out) noexcept {
    int const width = GetScreenWidth();
    int const height = GetScreenHeight();
    if (width <= 0 || height <= 0) {
        return false;
    }

    // Use vk::mat4_proj_view so 2D overlays match the lit 3D batch exactly.
    Mat4 const clip_from_world =
        vk::mat4_proj_view(camera, width, height);
    ClipHomogeneous const clip = vk::mat4_transform_homogeneous(clip_from_world, position);
    if (!vk::clip_homogeneous_visible(clip)) {
        return false;
    }

    return ProjectWorldToScreenForPick(position, camera, out);
}

Vector2 zh::gfx::GetWorldToScreen(Vector3 position, Camera3D camera) {
    Vector2 out{};
    if (TryGetWorldToScreen(position, camera, out)) {
        return out;
    }
    return Vector2{-10000.f, -10000.f};
}
