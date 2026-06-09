#include "zh/gfx/vulkan/vk_renderer.hpp"

#include "zh/platform/input.hpp"
#include "zh/platform/window.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdio>

namespace zh::gfx::vk {

namespace {

VkRenderer g_renderer{};

void framebuffer_resize_callback(GLFWwindow *, int, int) {
    global_renderer().notify_framebuffer_resized();
}

[[nodiscard]] VkClearValue to_vk_clear(Color c) noexcept {
    VkClearValue value{};
    value.color.float32[0] = static_cast<float>(c.r) / 255.f;
    value.color.float32[1] = static_cast<float>(c.g) / 255.f;
    value.color.float32[2] = static_cast<float>(c.b) / 255.f;
    value.color.float32[3] = static_cast<float>(c.a) / 255.f;
    return value;
}

void log_swapchain_recreate(char const *reason) noexcept {
    static int count = 0;
    std::fprintf(stderr, "[zh][gfx] swapchain recreate (%s) #%d\n", reason, ++count);
}

}  // namespace

VkRenderer &global_renderer() noexcept {
    return g_renderer;
}

bool VkRenderer::create_draw_pipelines() {
    if (!colored_pipeline_.create(context_, swapchain_, false)) {
        return false;
    }
    if (!lit_colored_pipeline_.create(context_, swapchain_)) {
        return false;
    }
    return font_.init(context_);
}

void VkRenderer::destroy_draw_pipelines() {
    font_.shutdown(context_);
    lit_colored_pipeline_.destroy(context_);
    colored_pipeline_.destroy(context_);
}

bool VkRenderer::init() {
    if (initialized_) {
        return true;
    }
    auto *window = platform::native_window();
    if (window == nullptr) {
        return false;
    }
    glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
    platform::init_input();

    if (!context_.init(window)) {
        return false;
    }
    if (!swapchain_.create(context_, window)) {
        context_.shutdown();
        return false;
    }
    if (!draw2d_.create_gpu(context_) || !draw3d_.create_gpu(context_) || !create_draw_pipelines() || !create_command_pool() ||
        !create_command_buffers() || !create_sync_objects()) {
        shutdown();
        return false;
    }
    reset_images_in_flight();
    initialized_ = true;
    return true;
}

void VkRenderer::shutdown() {
    if (!initialized_) {
        return;
    }
    vkDeviceWaitIdle(context_.device());
    destroy_sync_objects();
    destroy_command_buffers();
    destroy_command_pool();
    destroy_draw_pipelines();
    draw2d_.destroy_gpu(context_);
    draw3d_.destroy_gpu(context_);
    swapchain_.destroy(context_);
    context_.shutdown();
    initialized_ = false;
    framebuffer_resized_ = false;
    current_frame_ = 0;
    images_in_flight_.clear();
}

void VkRenderer::reset_images_in_flight() noexcept {
    images_in_flight_.assign(swapchain_.image_count(), VK_NULL_HANDLE);
}

void VkRenderer::recreate_swapchain_and_pipelines(GLFWwindow *window, char const *reason) {
    log_swapchain_recreate(reason);
    vkDeviceWaitIdle(context_.device());
    destroy_draw_pipelines();
    swapchain_.recreate(context_, window);
    create_draw_pipelines();
    reset_images_in_flight();
}

void VkRenderer::begin_frame() {
    platform::update_input_frame();
    draw2d_.reset();
    draw3d_.reset();
    draw2d_.set_screen_size(platform::framebuffer_width(), platform::framebuffer_height());
}

void VkRenderer::end_frame() {
    auto *window = platform::native_window();
    if (window == nullptr || !initialized_) {
        return;
    }

    if (framebuffer_resized_) {
        recreate_swapchain_and_pipelines(window, "resize");
        framebuffer_resized_ = false;
    }

    bool frame_presented = false;
    while (!frame_presented) {
        vkWaitForFences(context_.device(), 1, &in_flight_fences_[current_frame_], VK_TRUE,
                        UINT64_MAX);

        std::uint32_t image_index = 0;
        VkResult const acquire = vkAcquireNextImageKHR(
            context_.device(), swapchain_.handle(), UINT64_MAX,
            image_available_[current_frame_], VK_NULL_HANDLE, &image_index);

        if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swapchain_and_pipelines(window, "acquire-out-of-date");
            continue;
        }
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("vkAcquireNextImageKHR failed");
        }

        if (image_index < images_in_flight_.size() &&
            images_in_flight_[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(context_.device(), 1, &images_in_flight_[image_index], VK_TRUE,
                            UINT64_MAX);
        }

        vkResetFences(context_.device(), 1, &in_flight_fences_[current_frame_]);
        vkResetCommandBuffer(command_buffers_[current_frame_], 0);
        record_command_buffer(command_buffers_[current_frame_], image_index);

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &image_available_[current_frame_];
        submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command_buffers_[current_frame_];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &render_finished_[current_frame_];

        if (vkQueueSubmit(context_.graphics_queue(), 1, &submit,
                          in_flight_fences_[current_frame_]) != VK_SUCCESS) {
            throw std::runtime_error("vkQueueSubmit failed");
        }

        VkSwapchainKHR swapchains[] = {swapchain_.handle()};
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &render_finished_[current_frame_];
        present.swapchainCount = 1;
        present.pSwapchains = swapchains;
        present.pImageIndices = &image_index;

        VkResult const present_result = vkQueuePresentKHR(context_.present_queue(), &present);
        if (present_result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swapchain_and_pipelines(window, "present-out-of-date");
            continue;
        }
        if (present_result == VK_SUBOPTIMAL_KHR) {
            // Still valid to present; only recreate on explicit resize/out-of-date.
        } else if (present_result != VK_SUCCESS) {
            throw std::runtime_error("vkQueuePresentKHR failed");
        }

        if (image_index < images_in_flight_.size()) {
            images_in_flight_[image_index] = in_flight_fences_[current_frame_];
        }
        frame_presented = true;
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
    platform::end_input_frame();
}

bool VkRenderer::create_command_pool() {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = context_.queue_families().graphics.value();
    return vkCreateCommandPool(context_.device(), &info, nullptr, &command_pool_) == VK_SUCCESS;
}

void VkRenderer::destroy_command_pool() {
    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_.device(), command_pool_, nullptr);
        command_pool_ = VK_NULL_HANDLE;
    }
}

bool VkRenderer::create_sync_objects() {
    image_available_.resize(kMaxFramesInFlight);
    render_finished_.resize(kMaxFramesInFlight);
    in_flight_fences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(context_.device(), &sem_info, nullptr, &image_available_[i]) !=
                VK_SUCCESS ||
            vkCreateSemaphore(context_.device(), &sem_info, nullptr, &render_finished_[i]) !=
                VK_SUCCESS ||
            vkCreateFence(context_.device(), &fence_info, nullptr, &in_flight_fences_[i]) !=
                VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

void VkRenderer::destroy_sync_objects() {
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        vkDestroySemaphore(context_.device(), image_available_[i], nullptr);
        vkDestroySemaphore(context_.device(), render_finished_[i], nullptr);
        vkDestroyFence(context_.device(), in_flight_fences_[i], nullptr);
    }
    image_available_.clear();
    render_finished_.clear();
    in_flight_fences_.clear();
}

bool VkRenderer::create_command_buffers() {
    command_buffers_.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = command_pool_;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = static_cast<std::uint32_t>(command_buffers_.size());
    return vkAllocateCommandBuffers(context_.device(), &info, command_buffers_.data()) ==
           VK_SUCCESS;
}

void VkRenderer::destroy_command_buffers() {
    if (!command_buffers_.empty() && command_pool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(context_.device(), command_pool_,
                             static_cast<std::uint32_t>(command_buffers_.size()),
                             command_buffers_.data());
    }
    command_buffers_.clear();
}

void VkRenderer::record_command_buffer(VkCommandBuffer cmd, std::uint32_t image_index) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    auto const extent = swapchain_.extent();
    bool const use_msaa = swapchain_.msaa_samples() != VK_SAMPLE_COUNT_1_BIT;
    VkClearValue clears[3]{};
    clears[0] = to_vk_clear(clear_color_);
    if (use_msaa) {
        clears[1] = to_vk_clear(clear_color_);
        clears[2].depthStencil = {1.f, 0};
    } else {
        clears[1].depthStencil = {1.f, 0};
    }
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = swapchain_.render_pass();
    rp.framebuffer = swapchain_.framebuffer(image_index);
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = extent;
    rp.clearValueCount = use_msaa ? 3U : 2U;
    rp.pClearValues = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    draw2d_.flush_background(cmd, colored_pipeline_, extent);
    draw3d_.flush(cmd, context_, lit_colored_pipeline_, extent);
    draw2d_.flush(cmd, colored_pipeline_, extent);
    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
}

}  // namespace zh::gfx::vk
