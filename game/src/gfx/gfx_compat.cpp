#include "zh/gfx/gfx_compat.hpp"

#include "zh/gfx/gfx_api.hpp"
#include "zh/gfx/vulkan/vk_renderer.hpp"
#include "zh/platform/input.hpp"
#include "zh/platform/paths_win32.hpp"
#include "zh/platform/window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <random>

#include <GLFW/glfw3.h>

namespace {

unsigned g_config_flags = 0;

[[nodiscard]] zh::gfx::vk::Draw2dBatch &batch() {
    return zh::gfx::vk::global_renderer().draw2d();
}

[[nodiscard]] zh::gfx::vk::VkFont &font() {
    return zh::gfx::vk::global_renderer().font();
}

}  // namespace

void SetConfigFlags(unsigned flags) {
    g_config_flags = flags;
}

void InitWindow(int width, int height, char const *title) {
    (void)width;
    (void)height;
    if (!zh::platform::init_window(width, height, title)) {
        return;
    }
    if ((g_config_flags & FLAG_VSYNC_HINT) != 0) {
        glfwSwapInterval(1);
    }
    if (!zh::gfx::init(width, height, title)) {
        zh::platform::shutdown_window();
    }
}

void CloseWindow() {
    zh::gfx::shutdown();
}

bool WindowShouldClose() {
    return zh::platform::should_close();
}

void SetWindowMinSize(int min_w, int min_h) {
    zh::platform::set_min_size(min_w, min_h);
}

void SetTargetFPS(int /*fps*/) {}

void PollInputEvents() {
    zh::platform::poll_events();
    zh::platform::update_input_frame();
}

void BeginDrawing() {
    zh::gfx::vk::global_renderer().begin_frame();
}

void EndDrawing() {
    zh::gfx::vk::global_renderer().end_frame();
}

void ClearBackground(Color color) {
    zh::gfx::clear_background(color);
}

void DrawFPS(int, int) {}

int GetScreenWidth() {
    return zh::platform::framebuffer_width();
}

int GetScreenHeight() {
    return zh::platform::framebuffer_height();
}

float GetFrameTime() {
    return zh::platform::frame_time_seconds();
}

double GetTime() {
    return glfwGetTime();
}

bool IsKeyDown(int key) {
    return zh::platform::key_down(key);
}

bool IsKeyPressed(int key) {
    return zh::platform::key_pressed(key);
}

int GetCharPressed() {
    return zh::platform::char_pressed();
}

bool IsMouseButtonDown(int button) {
    return zh::platform::mouse_down(button);
}

bool IsMouseButtonPressed(int button) {
    return zh::platform::mouse_pressed(button);
}

Vector2 GetMousePosition() {
    return Vector2{zh::platform::mouse_x(), zh::platform::mouse_y()};
}

float GetMouseWheelMove() {
    return zh::platform::mouse_wheel();
}

int GetRandomValue(int min, int max) {
    if (min > max) {
        std::swap(min, max);
    }
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

void DrawRectangle(int x, int y, int width, int height, Color color) {
    batch().fill_rect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width),
                      static_cast<float>(height), color);
}

void DrawRectangleRec(Rectangle rec, Color color) {
    batch().fill_rect(rec.x, rec.y, rec.width, rec.height, color);
}

void DrawRectangleGradientV(int x, int y, int width, int height, Color top, Color bottom) {
    if (width <= 0 || height <= 0) {
        return;
    }
    int const bands = 32;
    float const band_h = static_cast<float>(height) / static_cast<float>(bands);
    for (int i = 0; i < bands; ++i) {
        float const t0 = static_cast<float>(i) / static_cast<float>(bands);
        float const t1 = static_cast<float>(i + 1) / static_cast<float>(bands);
        float const tm = (t0 + t1) * 0.5f;
        Color const c = Color{
            static_cast<unsigned char>(static_cast<float>(bottom.r) * (1.f - tm) +
                                       static_cast<float>(top.r) * tm),
            static_cast<unsigned char>(static_cast<float>(bottom.g) * (1.f - tm) +
                                       static_cast<float>(top.g) * tm),
            static_cast<unsigned char>(static_cast<float>(bottom.b) * (1.f - tm) +
                                       static_cast<float>(top.b) * tm),
            255};
        batch().fill_rect(static_cast<float>(x),
                          static_cast<float>(y) + band_h * static_cast<float>(i), static_cast<float>(width),
                          band_h + 1.f, c);
    }
}

void DrawPlayingSkyGradient() {
    int const w = GetScreenWidth();
    int const h = GetScreenHeight();
    batch().begin_background_layer();
    DrawRectangleGradientV(0, 0, w, h, Color{132, 158, 188, 255}, Color{196, 206, 218, 255});
    batch().end_background_layer();
}

void DrawMenuBackgroundGradient() {
    int const w = GetScreenWidth();
    int const h = GetScreenHeight();
    batch().begin_background_layer();
    DrawRectangleGradientV(0, 0, w, h, Color{12, 16, 28, 255}, Color{32, 42, 68, 255});
    batch().end_background_layer();
}

void DrawRectangleLinesEx(Rectangle rec, float line_thick, Color color) {
    batch().stroke_rect(rec.x, rec.y, rec.width, rec.height, line_thick, color);
}

void DrawLineEx(Vector2 start, Vector2 end, float thick, Color color) {
    batch().line(start, end, thick, color);
}

void DrawCircle(int centerX, int centerY, float radius, Color color) {
    batch().circle(Vector2{static_cast<float>(centerX), static_cast<float>(centerY)}, radius,
                   color);
}

void DrawCircleV(Vector2 center, float radius, Color color) {
    batch().circle(center, radius, color);
}

void DrawCircleLines(int centerX, int centerY, float radius, Color color) {
    batch().circle_outline(Vector2{static_cast<float>(centerX), static_cast<float>(centerY)},
                           radius, color);
}

void DrawCircleLinesV(Vector2 center, float radius, Color color) {
    batch().circle_outline(center, radius, color);
}

void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle,
              float endAngle, int segments, Color color) {
    batch().ring(center, innerRadius, outerRadius, startAngle, endAngle, segments, color);
}

void DrawText(char const *text, int posX, int posY, int fontSize, Color color) {
    font().draw_text(batch(), text, posX, posY, fontSize, color);
}

int MeasureText(char const *text, int fontSize) {
    return font().measure_text(text, fontSize);
}

bool CheckCollisionPointRec(Vector2 point, Rectangle rec) {
    return point.x >= rec.x && point.x <= (rec.x + rec.width) && point.y >= rec.y &&
           point.y <= (rec.y + rec.height);
}

Color ColorBrightness(Color color, float factor) {
    auto clamp_u8 = [](float v) -> unsigned char {
        return static_cast<unsigned char>(std::clamp(v, 0.f, 255.f));
    };
    if (factor < 0.f) {
        factor = 1.f + factor;
    }
    return Color{clamp_u8(static_cast<float>(color.r) * factor),
                 clamp_u8(static_cast<float>(color.g) * factor),
                 clamp_u8(static_cast<float>(color.b) * factor), color.a};
}

Color ColorFromHSV(float hue, float saturation, float value) {
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    int region = static_cast<int>(hue / 60.f) % 6;
    float f = hue / 60.f - static_cast<float>(region);
    float p = value * (1.f - saturation);
    float q = value * (1.f - f * saturation);
    float t = value * (1.f - (1.f - f) * saturation);
    switch (region) {
        case 0:
            r = value;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = value;
            b = p;
            break;
        case 2:
            r = p;
            g = value;
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = value;
            break;
        case 4:
            r = t;
            g = p;
            b = value;
            break;
        default:
            r = value;
            g = p;
            b = q;
            break;
    }
    return Color{static_cast<unsigned char>(r * 255.f), static_cast<unsigned char>(g * 255.f),
                 static_cast<unsigned char>(b * 255.f), 255};
}

Vector3 ColorToHSV(Color color) {
    float const rf = static_cast<float>(color.r) / 255.f;
    float const gf = static_cast<float>(color.g) / 255.f;
    float const bf = static_cast<float>(color.b) / 255.f;
    float const cmax = std::max({rf, gf, bf});
    float const cmin = std::min({rf, gf, bf});
    float diff = cmax - cmin;
    float hue = 0.f;
    if (diff > 1e-6f) {
        if (cmax == rf) {
            hue = std::fmod((gf - bf) / diff, 6.f);
        } else if (cmax == gf) {
            hue = (bf - rf) / diff + 2.f;
        } else {
            hue = (rf - gf) / diff + 4.f;
        }
        hue *= 60.f;
        if (hue < 0.f) {
            hue += 360.f;
        }
    }
    float const sat = (cmax <= 1e-6f) ? 0.f : diff / cmax;
    return Vector3{hue, sat, cmax};
}

void BeginMode3D(Camera3D camera) {
    zh::gfx::vk::global_renderer().draw3d().begin(camera);
}

void EndMode3D() {
    zh::gfx::vk::global_renderer().draw3d().end();
}

void DrawCylinder(Vector3 position, float radiusTop, float radiusBottom, float height, int slices,
                  Color color) {
    zh::gfx::vk::global_renderer().draw3d().cylinder(position, radiusTop, radiusBottom, height,
                                                       slices, color);
}

void DrawCube(Vector3 position, float width, float height, float depth, Color color) {
    zh::gfx::vk::global_renderer().draw3d().cube(position, width, height, depth, color);
}

void DrawTriangle3D(Vector3 v1, Vector3 v2, Vector3 v3, Color color) {
    zh::gfx::vk::global_renderer().draw3d().triangle(v1, v2, v3, color);
}
void EndShaderMode() {}
void rlDisableBackfaceCulling() {}
void rlEnableBackfaceCulling() {}

bool DirectoryExists(char const *dir) {
    if (dir == nullptr) {
        return false;
    }
    return std::filesystem::exists(dir) && std::filesystem::is_directory(dir);
}

bool FileExists(char const *fileName) {
    if (fileName == nullptr) {
        return false;
    }
    return std::filesystem::exists(fileName) && std::filesystem::is_regular_file(fileName);
}

void ChangeDirectory(char const *path) {
    if (path != nullptr) {
        std::filesystem::current_path(path);
    }
}

char const *GetApplicationDirectory() {
    return zh::platform::application_directory();
}

char const *GetWorkingDirectory() {
    static char path[512];
    auto const p = std::filesystem::current_path().string();
    std::snprintf(path, sizeof(path), "%s", p.c_str());
    return path;
}

void TraceLog(int logLevel, char const *text, ...) {
    (void)logLevel;
    va_list args;
    va_start(args, text);
    std::vfprintf(stderr, text, args);
    std::fputc('\n', stderr);
    va_end(args);
}
