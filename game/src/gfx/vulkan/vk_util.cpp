#include "zh/gfx/vulkan/vk_util.hpp"

namespace zh::gfx::vk {

VkShaderModule create_shader_module(VkDevice device, std::vector<std::uint8_t> const &code) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<std::uint32_t const *>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule failed");
    }
    return module;
}

void destroy_shader_module(VkDevice device, VkShaderModule module) noexcept {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, module, nullptr);
    }
}

VkSampleCountFlagBits max_usable_sample_count(VkPhysicalDevice device) noexcept {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device, &props);

    VkSampleCountFlags const color = props.limits.framebufferColorSampleCounts;
    VkSampleCountFlags const depth = props.limits.framebufferDepthSampleCounts;
    VkSampleCountFlags counts = color & depth;

    if (counts & VK_SAMPLE_COUNT_4_BIT) {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT) {
        return VK_SAMPLE_COUNT_2_BIT;
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

}  // namespace zh::gfx::vk
