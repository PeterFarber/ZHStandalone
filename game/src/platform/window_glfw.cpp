#include "zh/platform/window.hpp"

#include <GLFW/glfw3.h>

#include <chrono>

namespace zh::platform {

namespace {

GLFWwindow *g_window = nullptr;
std::chrono::steady_clock::time_point g_last_frame{};

}  // namespace

bool init_window(int width, int height, char const *title) {
    if (!glfwInit()) {
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    g_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (g_window == nullptr) {
        glfwTerminate();
        return false;
    }
    g_last_frame = std::chrono::steady_clock::now();
    return true;
}

void shutdown_window() {
    if (g_window != nullptr) {
        glfwDestroyWindow(g_window);
        g_window = nullptr;
    }
    glfwTerminate();
}

void poll_events() {
    glfwPollEvents();
}

bool should_close() {
    return g_window == nullptr || glfwWindowShouldClose(g_window);
}

GLFWwindow *native_window() noexcept {
    return g_window;
}

int framebuffer_width() {
    int w = 0;
    int h = 0;
    if (g_window != nullptr) {
        glfwGetFramebufferSize(g_window, &w, &h);
    }
    return w;
}

int framebuffer_height() {
    int w = 0;
    int h = 0;
    if (g_window != nullptr) {
        glfwGetFramebufferSize(g_window, &w, &h);
    }
    return h;
}

float frame_time_seconds() {
    auto const now = std::chrono::steady_clock::now();
    float const dt =
        std::chrono::duration<float>(now - g_last_frame).count();
    g_last_frame = now;
    return dt;
}

void set_min_size(int min_w, int min_h) {
    if (g_window != nullptr) {
        glfwSetWindowSizeLimits(g_window, min_w, min_h, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }
}

}  // namespace zh::platform
