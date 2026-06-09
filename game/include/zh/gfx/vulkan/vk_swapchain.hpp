#pragma once

#include "zh/gfx/vulkan/vk_context.hpp"

#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;

namespace zh::gfx::vk {

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

[[nodiscard]] SwapchainSupport query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface);

class VkSwapchain {
public:
    VkSwapchain() = default;
    ~VkSwapchain();

    VkSwapchain(VkSwapchain const &) = delete;
    VkSwapchain &operator=(VkSwapchain const &) = delete;

    bool create(VkContext const &ctx, GLFWwindow *window);
    void destroy(VkContext const &ctx);
    bool recreate(VkContext const &ctx, GLFWwindow *window);

    [[nodiscard]] VkSwapchainKHR handle() const noexcept { return swapchain_; }
    [[nodiscard]] VkRenderPass render_pass() const noexcept { return render_pass_; }
    [[nodiscard]] VkFormat depth_format() const noexcept { return depth_format_; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return extent_; }
    [[nodiscard]] VkSampleCountFlagBits msaa_samples() const noexcept { return msaa_samples_; }
    [[nodiscard]] std::size_t image_count() const noexcept { return framebuffers_.size(); }
    [[nodiscard]] VkFramebuffer framebuffer(std::size_t index) const noexcept {
        return framebuffers_[index];
    }

private:
    bool create_swapchain(VkContext const &ctx, GLFWwindow *window);
    void create_image_views(VkContext const &ctx);
    void create_msaa_color_resources(VkContext const &ctx);
    void create_depth_resources(VkContext const &ctx);
    void create_render_pass(VkContext const &ctx);
    void create_framebuffers(VkContext const &ctx);
    void cleanup_swapchain(VkContext const &ctx);

    [[nodiscard]] VkFormat choose_depth_format(VkContext const &ctx) const;

    [[nodiscard]] VkSurfaceFormatKHR choose_format() const;
    [[nodiscard]] VkPresentModeKHR choose_present_mode() const;
    [[nodiscard]] VkExtent2D choose_extent(GLFWwindow *window) const;

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    VkSwapchainKHR old_swapchain_{VK_NULL_HANDLE};
    VkFormat format_{VK_FORMAT_UNDEFINED};
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
    std::vector<VkFramebuffer> framebuffers_;
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    VkFormat depth_format_{VK_FORMAT_UNDEFINED};
    VkSampleCountFlagBits msaa_samples_{VK_SAMPLE_COUNT_1_BIT};
    std::vector<VkImage> depth_images_;
    std::vector<VkDeviceMemory> depth_memories_;
    std::vector<VkImageView> depth_views_;
    std::vector<VkImage> msaa_color_images_;
    std::vector<VkDeviceMemory> msaa_color_memories_;
    std::vector<VkImageView> msaa_color_views_;
    SwapchainSupport support_{};
};

}  // namespace zh::gfx::vk
