#pragma once

#include "zh/gfx/gfx_types.hpp"
#include "zh/gfx/vulkan/vk_context.hpp"
#include "zh/gfx/vulkan/vk_swapchain.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace zh::gfx::vk {

struct LitColoredVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float r;
    float g;
    float b;
    float a;
};

class VkLitColoredPipeline {
public:
    bool create(VkContext const &ctx, VkSwapchain const &swapchain);
    void destroy(VkContext const &ctx);
    void bind_and_set_viewport(VkCommandBuffer cmd, VkExtent2D extent) const;

    [[nodiscard]] VkPipeline pipeline() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }

private:
    [[nodiscard]] bool load_shaders(VkContext const &ctx, VkShaderModule &vert,
                                    VkShaderModule &frag) const;

    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
};

class Draw3dBatch {
public:
    static inline constexpr std::size_t kMaxVertices = 2097152;

    void reset() noexcept;
    void begin(Camera3D camera) noexcept;
    void end() noexcept;

    void triangle(Vector3 a, Vector3 b, Vector3 c, Color color);
    void cylinder(Vector3 center, float radius_top, float radius_bottom, float height, int slices,
                  Color color);
    void cube(Vector3 center, float width, float height, float depth, Color color);

    void flush(VkCommandBuffer cmd, VkContext const &ctx, VkLitColoredPipeline const &pipeline,
               VkExtent2D extent);

    [[nodiscard]] bool create_gpu(VkContext const &ctx);
    void destroy_gpu(VkContext const &ctx);

private:
    void push_triangle(LitColoredVertex const &a, LitColoredVertex const &b,
                       LitColoredVertex const &c);

    [[nodiscard]] static LitColoredVertex make_vertex(float x, float y, float z, float nx,
                                                      float ny, float nz, Color c) noexcept;

    Camera3D camera_{};
    bool camera_ready_{false};
    std::vector<LitColoredVertex> vertices_;
    VkBuffer vertex_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory vertex_memory_{VK_NULL_HANDLE};
    void *mapped_{nullptr};
};

}  // namespace zh::gfx::vk
