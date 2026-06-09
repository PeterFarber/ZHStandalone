#pragma once

#include <vulkan/vulkan.h>

#include <optional>

struct GLFWwindow;

namespace zh::gfx::vk {

struct QueueFamilies {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    [[nodiscard]] bool complete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

class VkContext {
public:
    VkContext() = default;
    ~VkContext();

    VkContext(VkContext const &) = delete;
    VkContext &operator=(VkContext const &) = delete;

    bool init(GLFWwindow *window);
    void shutdown();

    [[nodiscard]] VkInstance instance() const noexcept { return instance_; }
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept { return physical_device_; }
    [[nodiscard]] VkDevice device() const noexcept { return device_; }
    [[nodiscard]] VkQueue graphics_queue() const noexcept { return graphics_queue_; }
    [[nodiscard]] VkQueue present_queue() const noexcept { return present_queue_; }
    [[nodiscard]] VkSurfaceKHR surface() const noexcept { return surface_; }
    [[nodiscard]] QueueFamilies const &queue_families() const noexcept { return queue_families_; }

private:
    bool create_instance();
    bool setup_debug_messenger();
    bool create_surface(GLFWwindow *window);
    bool pick_physical_device();
    bool create_logical_device();

    GLFWwindow *window_{nullptr};
    VkInstance instance_{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debug_messenger_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue graphics_queue_{VK_NULL_HANDLE};
    VkQueue present_queue_{VK_NULL_HANDLE};
    QueueFamilies queue_families_{};
};

}  // namespace zh::gfx::vk
