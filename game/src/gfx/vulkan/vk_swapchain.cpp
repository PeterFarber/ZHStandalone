#include "zh/gfx/vulkan/vk_swapchain.hpp"

#include "zh/gfx/vulkan/vk_util.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace zh::gfx::vk {

namespace {

char const *kDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

[[nodiscard]] std::uint32_t find_device_local_memory_type(VkPhysicalDevice device,
                                                          std::uint32_t type_bits) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(device, &props);
    for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((type_bits & (1U << i)) &&
            (props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            return i;
        }
    }
    throw std::runtime_error("device-local memory type not found");
}

void create_image_view(VkDevice device, VkImage image, VkFormat format,
                       VkImageAspectFlags aspect, VkImageView &out_view) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &view_info, nullptr, &out_view) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView failed");
    }
}

}  // namespace

SwapchainSupport query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport details{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    std::uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    if (format_count > 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
    }

    std::uint32_t present_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count, nullptr);
    if (present_count > 0) {
        details.present_modes.resize(present_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_count,
                                                   details.present_modes.data());
    }
    return details;
}

VkSwapchain::~VkSwapchain() {
    swapchain_ = VK_NULL_HANDLE;
}

bool VkSwapchain::create(VkContext const &ctx, GLFWwindow *window) {
    support_ = query_swapchain_support(ctx.physical_device(), ctx.surface());
    if (support_.formats.empty() || support_.present_modes.empty()) {
        return false;
    }
    if (!create_swapchain(ctx, window)) {
        return false;
    }
    create_image_views(ctx);
    msaa_samples_ = max_usable_sample_count(ctx.physical_device());
    create_msaa_color_resources(ctx);
    create_depth_resources(ctx);
    create_render_pass(ctx);
    create_framebuffers(ctx);
    return true;
}

void VkSwapchain::destroy(VkContext const &ctx) {
    cleanup_swapchain(ctx);
}

bool VkSwapchain::recreate(VkContext const &ctx, GLFWwindow *window) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx.device());
    VkSwapchainKHR const old = swapchain_;
    swapchain_ = VK_NULL_HANDLE;
    cleanup_swapchain(ctx);
    old_swapchain_ = old;
    return create(ctx, window);
}

bool VkSwapchain::create_swapchain(VkContext const &ctx, GLFWwindow *window) {
    auto const surface_format = choose_format();
    auto const present_mode = choose_present_mode();
    extent_ = choose_extent(window);
    format_ = surface_format.format;

    std::uint32_t image_count = support_.capabilities.minImageCount + 1;
    if (support_.capabilities.maxImageCount > 0 &&
        image_count > support_.capabilities.maxImageCount) {
        image_count = support_.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = ctx.surface();
    info.minImageCount = image_count;
    info.imageFormat = surface_format.format;
    info.imageColorSpace = surface_format.colorSpace;
    info.imageExtent = extent_;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = support_.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = present_mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = old_swapchain_;

    auto const &qf = ctx.queue_families();
    std::uint32_t indices[] = {qf.graphics.value(), qf.present.value()};
    if (qf.graphics != qf.present) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = indices;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(ctx.device(), &info, nullptr, &swapchain_) != VK_SUCCESS) {
        return false;
    }

    if (old_swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx.device(), old_swapchain_, nullptr);
        old_swapchain_ = VK_NULL_HANDLE;
    }

    vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &image_count, nullptr);
    images_.resize(image_count);
    vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &image_count, images_.data());
    return true;
}

void VkSwapchain::create_image_views(VkContext const &ctx) {
    image_views_.resize(images_.size());
    for (std::size_t i = 0; i < images_.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = images_[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format_;
        info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(ctx.device(), &info, nullptr, &image_views_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateImageView failed");
        }
    }
}

void VkSwapchain::create_msaa_color_resources(VkContext const &ctx) {
    if (msaa_samples_ == VK_SAMPLE_COUNT_1_BIT) {
        return;
    }

    std::size_t const count = image_views_.size();
    msaa_color_images_.resize(count);
    msaa_color_memories_.resize(count);
    msaa_color_views_.resize(count);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = extent_.width;
    image_info.extent.height = extent_.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format_;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image_info.samples = msaa_samples_;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    for (std::size_t i = 0; i < count; ++i) {
        if (vkCreateImage(ctx.device(), &image_info, nullptr, &msaa_color_images_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateImage msaa color failed");
        }

        VkMemoryRequirements mem_req{};
        vkGetImageMemoryRequirements(ctx.device(), msaa_color_images_[i], &mem_req);

        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = mem_req.size;
        alloc.memoryTypeIndex = find_device_local_memory_type(ctx.physical_device(), mem_req.memoryTypeBits);

        if (vkAllocateMemory(ctx.device(), &alloc, nullptr, &msaa_color_memories_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateMemory msaa color failed");
        }
        vkBindImageMemory(ctx.device(), msaa_color_images_[i], msaa_color_memories_[i], 0);
        create_image_view(ctx.device(), msaa_color_images_[i], format_, VK_IMAGE_ASPECT_COLOR_BIT,
                          msaa_color_views_[i]);
    }
}

void VkSwapchain::create_depth_resources(VkContext const &ctx) {
    depth_format_ = choose_depth_format(ctx);

    std::size_t const count = image_views_.size();
    depth_images_.resize(count);
    depth_memories_.resize(count);
    depth_views_.resize(count);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = extent_.width;
    image_info.extent.height = extent_.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = depth_format_;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.samples = msaa_samples_;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    for (std::size_t i = 0; i < count; ++i) {
        if (vkCreateImage(ctx.device(), &image_info, nullptr, &depth_images_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateImage depth failed");
        }

        VkMemoryRequirements mem_req{};
        vkGetImageMemoryRequirements(ctx.device(), depth_images_[i], &mem_req);

        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = mem_req.size;
        alloc.memoryTypeIndex = find_device_local_memory_type(ctx.physical_device(), mem_req.memoryTypeBits);

        if (vkAllocateMemory(ctx.device(), &alloc, nullptr, &depth_memories_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateMemory depth failed");
        }
        vkBindImageMemory(ctx.device(), depth_images_[i], depth_memories_[i], 0);
        create_image_view(ctx.device(), depth_images_[i], depth_format_, VK_IMAGE_ASPECT_DEPTH_BIT,
                          depth_views_[i]);
    }
}

VkFormat VkSwapchain::choose_depth_format(VkContext const &ctx) const {
    VkFormat const candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT,
                                   VK_FORMAT_D16_UNORM};
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(ctx.physical_device(), format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    return VK_FORMAT_D32_SFLOAT;
}

void VkSwapchain::create_render_pass(VkContext const &ctx) {
    bool const use_msaa = msaa_samples_ != VK_SAMPLE_COUNT_1_BIT;

    VkAttachmentDescription color{};
    color.format = format_;
    color.samples = use_msaa ? msaa_samples_ : VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = use_msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout =
        use_msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription color_resolve{};
    color_resolve.format = format_;
    color_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    color_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = depth_format_;
    depth.samples = use_msaa ? msaa_samples_ : VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_resolve_ref{};
    color_resolve_ref.attachment = 1;
    color_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = use_msaa ? 2U : 1U;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription attachments[3] = {color, color_resolve, depth};
    std::uint32_t attachment_count = use_msaa ? 3U : 2U;
    if (!use_msaa) {
        attachments[1] = depth;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;
    if (use_msaa) {
        subpass.pResolveAttachments = &color_resolve_ref;
    }

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    // VulkanTutorial ch. 27: depth clear must complete before depth-tested 3D draw.
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = attachment_count;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(ctx.device(), &info, nullptr, &render_pass_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateRenderPass failed");
    }
}

void VkSwapchain::create_framebuffers(VkContext const &ctx) {
    bool const use_msaa = msaa_samples_ != VK_SAMPLE_COUNT_1_BIT;
    framebuffers_.resize(image_views_.size());
    for (std::size_t i = 0; i < image_views_.size(); ++i) {
        VkImageView attachments[3];
        std::uint32_t attachment_count = 0;
        if (use_msaa) {
            attachments[attachment_count++] = msaa_color_views_[i];
            attachments[attachment_count++] = image_views_[i];
            attachments[attachment_count++] = depth_views_[i];
        } else {
            attachments[attachment_count++] = image_views_[i];
            attachments[attachment_count++] = depth_views_[i];
        }

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = render_pass_;
        info.attachmentCount = attachment_count;
        info.pAttachments = attachments;
        info.width = extent_.width;
        info.height = extent_.height;
        info.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateFramebuffer failed");
        }
    }
}

void VkSwapchain::cleanup_swapchain(VkContext const &ctx) {
    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(ctx.device(), fb, nullptr);
    }
    framebuffers_.clear();

    for (VkImageView view : msaa_color_views_) {
        vkDestroyImageView(ctx.device(), view, nullptr);
    }
    msaa_color_views_.clear();
    for (VkImage image : msaa_color_images_) {
        vkDestroyImage(ctx.device(), image, nullptr);
    }
    msaa_color_images_.clear();
    for (VkDeviceMemory memory : msaa_color_memories_) {
        vkFreeMemory(ctx.device(), memory, nullptr);
    }
    msaa_color_memories_.clear();

    for (VkImageView view : depth_views_) {
        vkDestroyImageView(ctx.device(), view, nullptr);
    }
    depth_views_.clear();
    for (VkImage image : depth_images_) {
        vkDestroyImage(ctx.device(), image, nullptr);
    }
    depth_images_.clear();
    for (VkDeviceMemory memory : depth_memories_) {
        vkFreeMemory(ctx.device(), memory, nullptr);
    }
    depth_memories_.clear();

    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx.device(), render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }

    for (auto view : image_views_) {
        vkDestroyImageView(ctx.device(), view, nullptr);
    }
    image_views_.clear();
    images_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx.device(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

VkSurfaceFormatKHR VkSwapchain::choose_format() const {
    for (auto const &f : support_.formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return support_.formats.front();
}

VkPresentModeKHR VkSwapchain::choose_present_mode() const {
    // FIFO = vsync; avoids mailbox/image reuse races on some Windows drivers.
    for (auto const mode : support_.present_modes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            return mode;
        }
    }
    for (auto const mode : support_.present_modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return support_.present_modes.front();
}

VkExtent2D VkSwapchain::choose_extent(GLFWwindow *window) const {
    if (support_.capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return support_.capabilities.currentExtent;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    VkExtent2D extent{
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height),
    };
    extent.width = std::clamp(extent.width, support_.capabilities.minImageExtent.width,
                              support_.capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, support_.capabilities.minImageExtent.height,
                               support_.capabilities.maxImageExtent.height);
    return extent;
}

}  // namespace zh::gfx::vk
