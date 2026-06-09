#include "zh/platform/input.hpp"

#include "zh/platform/window.hpp"

#include <GLFW/glfw3.h>

#include <climits>
#include <deque>
#include <iterator>

namespace zh::platform {

namespace {

std::deque<int> g_char_queue;
bool g_keys_down[512]{};
bool g_keys_pressed[512]{};
bool g_mouse_down[8]{};
bool g_mouse_pressed[8]{};
float g_mouse_x = 0.f;
float g_mouse_y = 0.f;
float g_mouse_wheel = 0.f;

void char_callback(GLFWwindow *, unsigned int codepoint) {
    if (codepoint <= static_cast<unsigned int>(INT_MAX)) {
        g_char_queue.push_back(static_cast<int>(codepoint));
    }
}

void key_callback(GLFWwindow *, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key < 0 || key >= static_cast<int>(std::size(g_keys_down))) {
        return;
    }
    if (action == GLFW_PRESS) {
        g_keys_down[key] = true;
        g_keys_pressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        g_keys_down[key] = false;
    }
}

void mouse_button_callback(GLFWwindow *, int button, int action, int /*mods*/) {
    if (button < 0 || button >= static_cast<int>(std::size(g_mouse_down))) {
        return;
    }
    if (action == GLFW_PRESS) {
        g_mouse_down[button] = true;
        g_mouse_pressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        g_mouse_down[button] = false;
    }
}

void scroll_callback(GLFWwindow *, double /*xoffset*/, double yoffset) {
    g_mouse_wheel += static_cast<float>(yoffset);
}

}  // namespace

void init_input() {
    auto *window = native_window();
    if (window == nullptr) {
        return;
    }
    glfwSetCharCallback(window, char_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
}

void update_input_frame() {
    auto *window = native_window();
    if (window == nullptr) {
        return;
    }
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);

    int win_w = 0;
    int win_h = 0;
    int fb_w = 0;
    int fb_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    if (win_w > 0 && win_h > 0 && fb_w > 0 && fb_h > 0) {
        g_mouse_x = static_cast<float>(x) * static_cast<float>(fb_w) / static_cast<float>(win_w);
        g_mouse_y = static_cast<float>(y) * static_cast<float>(fb_h) / static_cast<float>(win_h);
    } else {
        g_mouse_x = static_cast<float>(x);
        g_mouse_y = static_cast<float>(y);
    }
}

void end_input_frame() {
    g_mouse_wheel = 0.f;
    std::fill(std::begin(g_keys_pressed), std::end(g_keys_pressed), false);
    std::fill(std::begin(g_mouse_pressed), std::end(g_mouse_pressed), false);
}

bool key_down(int key) {
    if (key < 0 || key >= static_cast<int>(std::size(g_keys_down))) {
        return false;
    }
    return g_keys_down[key];
}

bool key_pressed(int key) {
    if (key < 0 || key >= static_cast<int>(std::size(g_keys_pressed))) {
        return false;
    }
    bool const hit = g_keys_pressed[key];
    g_keys_pressed[key] = false;
    return hit;
}

int char_pressed() {
    if (g_char_queue.empty()) {
        return 0;
    }
    int const c = g_char_queue.front();
    g_char_queue.pop_front();
    return c;
}

bool mouse_down(int button) {
    if (button < 0 || button >= static_cast<int>(std::size(g_mouse_down))) {
        return false;
    }
    return g_mouse_down[button];
}

bool mouse_pressed(int button) {
    if (button < 0 || button >= static_cast<int>(std::size(g_mouse_pressed))) {
        return false;
    }
    bool const hit = g_mouse_pressed[button];
    g_mouse_pressed[button] = false;
    return hit;
}

float mouse_x() {
    return g_mouse_x;
}

float mouse_y() {
    return g_mouse_y;
}

float mouse_wheel() {
    return g_mouse_wheel;
}

}  // namespace zh::platform
