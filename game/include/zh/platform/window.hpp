#pragma once

struct GLFWwindow;

namespace zh::platform {

[[nodiscard]] bool init_window(int width, int height, char const *title);
void shutdown_window();
void poll_events();
[[nodiscard]] bool should_close();
[[nodiscard]] GLFWwindow *native_window() noexcept;

[[nodiscard]] int framebuffer_width();
[[nodiscard]] int framebuffer_height();
[[nodiscard]] float frame_time_seconds();

void set_min_size(int min_w, int min_h);

}  // namespace zh::platform
