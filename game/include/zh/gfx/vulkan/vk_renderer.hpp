#pragma once

#include "zh/gfx/gfx_types.hpp"
#include "zh/gfx/vulkan/vk_context.hpp"
#include "zh/gfx/vulkan/vk_draw2d.hpp"
#include "zh/gfx/vulkan/vk_draw3d.hpp"
#include "zh/gfx/vulkan/vk_font.hpp"
#include "zh/gfx/vulkan/vk_swapchain.hpp"

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace zh::gfx::vk {

inline constexpr int kMaxFramesInFlight = 2;

class VkRenderer {
public:
    bool init();
    void shutdown();

    void set_clear_color(Color color) noexcept { clear_color_ = color; }
    void begin_frame();
    void end_frame();

    [[nodiscard]] Draw2dBatch &draw2d() noexcept { return draw2d_; }
    [[nodiscard]] Draw3dBatch &draw3d() noexcept { return draw3d_; }
    [[nodiscard]] VkFont &font() noexcept { return font_; }

    void notify_framebuffer_resized() noexcept { framebuffer_resized_ = true; }

private:
    bool create_command_pool();
    void destroy_command_pool();
    bool create_sync_objects();
    void destroy_sync_objects();
    bool create_command_buffers();
    void destroy_command_buffers();
    bool create_draw_pipelines();
    void destroy_draw_pipelines();
    void recreate_swapchain_and_pipelines(GLFWwindow *window, char const *reason);
    void reset_images_in_flight() noexcept;
    void record_command_buffer(VkCommandBuffer cmd, std::uint32_t image_index);

    VkContext context_{};
    VkSwapchain swapchain_{};
    Draw2dBatch draw2d_{};
    Draw3dBatch draw3d_{};
    VkColoredPipeline colored_pipeline_{};
    VkLitColoredPipeline lit_colored_pipeline_{};
    VkFont font_{};
    VkCommandPool command_pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_;
    std::vector<VkSemaphore> render_finished_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;
    std::size_t current_frame_{0};
    Color clear_color_{15, 20, 32, 255};
    bool initialized_{false};
    bool framebuffer_resized_{false};
};

[[nodiscard]] VkRenderer &global_renderer() noexcept;

}  // namespace zh::gfx::vk
