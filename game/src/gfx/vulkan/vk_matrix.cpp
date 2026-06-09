#include "zh/gfx/vulkan/vk_matrix.hpp"

#include "zh/gfx/gfx_constants.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <cmath>
#include <cstring>

namespace zh::gfx::vk {

namespace {

inline float &at(Mat4 &m, int col, int row) noexcept {
    return m.m[col * 4 + row];
}

inline float at(Mat4 const &m, int col, int row) noexcept {
    return m.m[col * 4 + row];
}

[[nodiscard]] Mat4 mat4_from_glm(glm::mat4 const &g) noexcept {
    Mat4 m{};
    std::memcpy(m.m, &g[0][0], sizeof(m.m));
    return m;
}

[[nodiscard]] glm::vec3 to_glm(Vector3 v) noexcept {
    return glm::vec3{v.x, v.y, v.z};
}

}  // namespace

Mat4 mat4_identity() noexcept {
    Mat4 m{};
    at(m, 0, 0) = 1.f;
    at(m, 1, 1) = 1.f;
    at(m, 2, 2) = 1.f;
    at(m, 3, 3) = 1.f;
    return m;
}

Mat4 mat4_multiply(Mat4 a, Mat4 b) noexcept {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            at(r, col, row) = at(a, 0, row) * at(b, col, 0) + at(a, 1, row) * at(b, col, 1) +
                              at(a, 2, row) * at(b, col, 2) + at(a, 3, row) * at(b, col, 3);
        }
    }
    return r;
}

Mat4 mat4_invert(Mat4 mat) noexcept {
    glm::mat4 g{};
    std::memcpy(&g[0][0], mat.m, sizeof(mat.m));
    glm::mat4 const inv = glm::inverse(g);
    return mat4_from_glm(inv);
}

Mat4 mat4_translate(float x, float y, float z) noexcept {
    Mat4 m = mat4_identity();
    at(m, 3, 0) = x;
    at(m, 3, 1) = y;
    at(m, 3, 2) = z;
    return m;
}

Mat4 mat4_rotate_z(float radians) noexcept {
    Mat4 m = mat4_identity();
    float const c = std::cos(radians);
    float const s = std::sin(radians);
    at(m, 0, 0) = c;
    at(m, 0, 1) = s;
    at(m, 1, 0) = -s;
    at(m, 1, 1) = c;
    return m;
}

Mat4 mat4_scale(float x, float y, float z) noexcept {
    Mat4 m = mat4_identity();
    at(m, 0, 0) = x;
    at(m, 1, 1) = y;
    at(m, 2, 2) = z;
    return m;
}

Mat4 mat4_look_at(Vector3 eye, Vector3 target, Vector3 up) noexcept {
    glm::mat4 const view =
        glm::lookAt(to_glm(eye), to_glm(target), to_glm(up));
    return mat4_from_glm(view);
}

Mat4 mat4_perspective_vulkan(float fovy_rad, float aspect, float near_plane,
                             float far_plane) noexcept {
    glm::mat4 proj = glm::perspective(fovy_rad, aspect, near_plane, far_plane);
    // VulkanTutorial ch. 22/27: flip clip Y; keep glm OpenGL-style Z (works with our depth clear).
    proj[1][1] *= -1.f;
    return mat4_from_glm(proj);
}

Mat4 mat4_ortho_top_left(float width, float height) noexcept {
    Mat4 m = mat4_identity();
    at(m, 0, 0) = 2.f / width;
    at(m, 1, 1) = 2.f / height;
    at(m, 2, 2) = -1.f;
    at(m, 3, 0) = -1.f;
    at(m, 3, 1) = -1.f;
    return m;
}

Mat4 mat4_proj_view(Camera3D const &camera, int width, int height) noexcept {
    Mat4 const view = mat4_look_at(camera.position, camera.target, camera.up);
    if (camera.projection != CAMERA_PERSPECTIVE) {
        return view;
    }
    float const aspect =
        static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    Mat4 const proj =
        mat4_perspective_vulkan(camera.fovy * DEG2RAD, aspect, kCameraNearPlane, kCameraFarPlane);
    return mat4_multiply(proj, view);
}

Mat4 mat4_camera_2d(Camera2D camera) noexcept {
    Mat4 const origin = mat4_translate(-camera.target.x, -camera.target.y, 0.f);
    Mat4 const rotation = mat4_rotate_z(camera.rotation * DEG2RAD);
    Mat4 const scale = mat4_scale(camera.zoom, camera.zoom, 1.f);
    Mat4 const offset = mat4_translate(camera.offset.x, camera.offset.y, 0.f);
    return mat4_multiply(mat4_multiply(origin, mat4_multiply(scale, rotation)), offset);
}

Vector3 mat4_transform_point(Mat4 m, Vector3 v) noexcept {
    float const x = at(m, 0, 0) * v.x + at(m, 1, 0) * v.y + at(m, 2, 0) * v.z + at(m, 3, 0);
    float const y = at(m, 0, 1) * v.x + at(m, 1, 1) * v.y + at(m, 2, 1) * v.z + at(m, 3, 1);
    float const z = at(m, 0, 2) * v.x + at(m, 1, 2) * v.y + at(m, 2, 2) * v.z + at(m, 3, 2);
    float const w = at(m, 0, 3) * v.x + at(m, 1, 3) * v.y + at(m, 2, 3) * v.z + at(m, 3, 3);
    if (std::fabs(w) <= 1e-6f) {
        return Vector3{x, y, z};
    }
    float const inv_w = 1.f / w;
    return Vector3{x * inv_w, y * inv_w, z * inv_w};
}

ClipHomogeneous mat4_transform_homogeneous(Mat4 m, Vector3 v) noexcept {
    return ClipHomogeneous{
        at(m, 0, 0) * v.x + at(m, 1, 0) * v.y + at(m, 2, 0) * v.z + at(m, 3, 0),
        at(m, 0, 1) * v.x + at(m, 1, 1) * v.y + at(m, 2, 1) * v.z + at(m, 3, 1),
        at(m, 0, 2) * v.x + at(m, 1, 2) * v.y + at(m, 2, 2) * v.z + at(m, 3, 2),
        at(m, 0, 3) * v.x + at(m, 1, 3) * v.y + at(m, 2, 3) * v.z + at(m, 3, 3),
    };
}

bool clip_homogeneous_visible(ClipHomogeneous clip) noexcept {
    if (clip.w <= 1e-4f) {
        return false;
    }
    float const inv_w = 1.f / clip.w;
    float const ndc_x = clip.x * inv_w;
    float const ndc_y = clip.y * inv_w;
    float const ndc_z = clip.z * inv_w;
    if (ndc_z < -1.5f || ndc_z > 1.5f) {
        return false;
    }
    return ndc_x >= -1.25f && ndc_x <= 1.25f && ndc_y >= -1.25f && ndc_y <= 1.25f;
}

Vector3 mat4_unproject_ndc(Vector3 ndc, Mat4 view, Mat4 proj) noexcept {
    Mat4 const inv = mat4_invert(mat4_multiply(proj, view));
    return mat4_transform_point(inv, ndc);
}

void mat4_to_glsl(Mat4 const &m, float out[16]) noexcept {
    std::memcpy(out, m.m, sizeof(m.m));
}

}  // namespace zh::gfx::vk
