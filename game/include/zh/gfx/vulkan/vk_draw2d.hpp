#pragma once

#include "zh/gfx/gfx_types.hpp"
#include "zh/gfx/vulkan/vk_context.hpp"
#include "zh/gfx/vulkan/vk_swapchain.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace zh::gfx::vk {

struct ColoredVertex {
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float a;
};

struct ColoredDrawCmd {
    std::uint32_t first_vertex{};
    std::uint32_t vertex_count{};
};

class VkColoredPipeline {
public:
    bool create(VkContext const &ctx, VkSwapchain const &swapchain, bool depth_test = false);
    void destroy(VkContext const &ctx);
    void bind_and_set_viewport(VkCommandBuffer cmd, VkExtent2D extent,
                               bool push_ortho_mvp = true) const;

    [[nodiscard]] VkPipeline pipeline() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }

private:
    [[nodiscard]] bool load_shaders(VkContext const &ctx, VkShaderModule &vert,
                                    VkShaderModule &frag) const;

    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
};

class Draw2dBatch {
public:
    static inline constexpr std::size_t kMaxVertices = 524288;

    void reset() noexcept;
    void set_screen_size(int width, int height) noexcept;

    void fill_rect(float x, float y, float w, float h, Color color);
    void stroke_rect(float x, float y, float w, float h, float thickness, Color color);
    void line(Vector2 from, Vector2 to, float thickness, Color color);
    void circle(Vector2 center, float radius, Color color, int segments = 32);
    void circle_outline(Vector2 center, float radius, Color color, int segments = 32);
    void ring(Vector2 center, float inner, float outer, float start_deg, float end_deg,
              int segments, Color color);

    void begin_background_layer() noexcept;
    void end_background_layer() noexcept;

    void flush_background(VkCommandBuffer cmd, VkColoredPipeline const &colored_pipe,
                          VkExtent2D extent);

    void flush(VkCommandBuffer cmd, VkColoredPipeline const &colored_pipe, VkExtent2D extent);

    [[nodiscard]] bool create_gpu(VkContext const &ctx);
    void destroy_gpu(VkContext const &ctx);

private:
    void push_vertex(float x, float y, Color color);
    void push_triangle(ColoredVertex const &a, ColoredVertex const &b, ColoredVertex const &c);
    void append_colored_cmd();

    [[nodiscard]] static ColoredVertex make_vertex(float x, float y, Color c) noexcept;

    int screen_w_{1};
    int screen_h_{1};
    bool background_layer_{false};
    std::vector<ColoredVertex> colored_vertices_;
    std::vector<ColoredDrawCmd> ops_;
    std::vector<ColoredVertex> bg_colored_vertices_;
    std::vector<ColoredDrawCmd> bg_ops_;
    VkBuffer colored_vertex_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory colored_vertex_memory_{VK_NULL_HANDLE};
    void *colored_mapped_{nullptr};
};

}  // namespace zh::gfx::vk
