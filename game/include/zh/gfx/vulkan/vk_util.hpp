#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace zh::gfx::vk {

[[nodiscard]] inline bool vk_ok(VkResult result, char const *what) {
    if (result != VK_SUCCESS) {
        return false;
    }
    (void)what;
    return true;
}

[[nodiscard]] inline std::vector<std::uint8_t> read_binary_file(char const *path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("failed to open file: ") + path);
    }
    auto const size = file.tellg();
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(buffer.data()), size);
    return buffer;
}

[[nodiscard]] VkShaderModule create_shader_module(VkDevice device,
                                                  std::vector<std::uint8_t> const &code);

void destroy_shader_module(VkDevice device, VkShaderModule module) noexcept;

[[nodiscard]] VkSampleCountFlagBits max_usable_sample_count(VkPhysicalDevice device) noexcept;

// Viewport + scissor for dynamic pipeline state (VulkanTutorial ch. 14 / 27).
// Uses the tutorial's default viewport (y=0, positive height) — do not flip here.
// Compensate for Vulkan vs OpenGL clip Y in projection/ortho instead (ch. 22: proj[1][1] *= -1).
inline void set_viewport_top_left(VkCommandBuffer cmd, VkExtent2D extent) noexcept {
    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

}  // namespace zh::gfx::vk
