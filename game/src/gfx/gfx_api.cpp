#include "zh/gfx/gfx_api.hpp"
#include "zh/gfx/vulkan/vk_renderer.hpp"
#include "zh/platform/window.hpp"

namespace zh::gfx {

bool init(int width, int height, char const *title) {
    (void)width;
    (void)height;
    (void)title;
    return vk::global_renderer().init();
}

void shutdown() {
    vk::global_renderer().shutdown();
    platform::shutdown_window();
}

void begin_drawing() {
    vk::global_renderer().begin_frame();
}

void end_drawing() {
    vk::global_renderer().end_frame();
}

void clear_background(Color color) {
    vk::global_renderer().set_clear_color(color);
}

}  // namespace zh::gfx
