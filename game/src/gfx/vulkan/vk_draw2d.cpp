#include "zh/gfx/vulkan/vk_draw2d.hpp"

#include "zh/gfx/gfx_constants.hpp"
#include "zh/gfx/vulkan/vk_matrix.hpp"
#include "zh/gfx/vulkan/vk_util.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace zh::gfx::vk {

namespace {

[[nodiscard]] char const *resolve_shader_spv(char const *name) {
    static char path[512];
    auto try_path = [&](char const *base) -> char const * {
        auto const full = std::filesystem::path(base) / "resources" / "shaders" / "vk" / name;
        if (std::filesystem::exists(full)) {
            auto const s = full.string();
            std::snprintf(path, sizeof(path), "%s", s.c_str());
            return path;
        }
        return nullptr;
    };
    if (char const *hit = try_path(".")) {
        return hit;
    }
    if (char const *hit = try_path("..")) {
        return hit;
    }
    return name;
}

[[nodiscard]] std::uint32_t find_memory_type(VkPhysicalDevice device, std::uint32_t type_bits,
                                             VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(device, &mem);
    for (std::uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if ((type_bits & (1U << i)) && (mem.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw std::runtime_error("find_memory_type failed");
}

struct PushConstants {
    float mvp[16];
};

}  // namespace

bool VkColoredPipeline::load_shaders(VkContext const &ctx, VkShaderModule &vert,
                                     VkShaderModule &frag) const {
    auto const vert_bytes = read_binary_file(resolve_shader_spv("colored.vert.spv"));
    auto const frag_bytes = read_binary_file(resolve_shader_spv("colored.frag.spv"));
    vert = create_shader_module(ctx.device(), vert_bytes);
    frag = create_shader_module(ctx.device(), frag_bytes);
    return vert != VK_NULL_HANDLE && frag != VK_NULL_HANDLE;
}

bool VkColoredPipeline::create(VkContext const &ctx, VkSwapchain const &swapchain,
                               bool const depth_test) {
    VkShaderModule vert = VK_NULL_HANDLE;
    VkShaderModule frag = VK_NULL_HANDLE;
    if (!load_shaders(ctx, vert, frag)) {
        return false;
    }

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &layout_info, nullptr, &layout_) != VK_SUCCESS) {
        destroy_shader_module(ctx.device(), vert);
        destroy_shader_module(ctx.device(), frag);
        return false;
    }

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(ColoredVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(ColoredVertex, x);
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[1].offset = offsetof(ColoredVertex, r);

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 2;
    vertex_input.pVertexAttributeDescriptions = attrs;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkPipelineInputAssemblyStateCreateInfo input_asm{};
    input_asm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_asm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = swapchain.msaa_samples();

    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = depth_test ? VK_TRUE : VK_FALSE;
    depth.depthWriteEnable = depth_test ? VK_TRUE : VK_FALSE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blend_attach{};
    blend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attach.blendEnable = VK_TRUE;
    blend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_attach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attach.colorBlendOp = VK_BLEND_OP_ADD;
    blend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attach.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attach;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipe_info{};
    pipe_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_info.stageCount = 2;
    pipe_info.pStages = stages;
    pipe_info.pVertexInputState = &vertex_input;
    pipe_info.pInputAssemblyState = &input_asm;
    pipe_info.pViewportState = &viewport_state;
    pipe_info.pRasterizationState = &raster;
    pipe_info.pMultisampleState = &msaa;
    pipe_info.pDepthStencilState = &depth;
    pipe_info.pColorBlendState = &blend;
    pipe_info.pDynamicState = &dynamic;
    pipe_info.layout = layout_;
    pipe_info.renderPass = swapchain.render_pass();
    pipe_info.subpass = 0;

    bool ok = vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &pipe_info, nullptr,
                                        &pipeline_) == VK_SUCCESS;
    destroy_shader_module(ctx.device(), vert);
    destroy_shader_module(ctx.device(), frag);
    return ok;
}

void VkColoredPipeline::destroy(VkContext const &ctx) {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx.device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx.device(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
}

void VkColoredPipeline::bind_and_set_viewport(VkCommandBuffer cmd, VkExtent2D extent,
                                              bool const push_ortho_mvp) const {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    set_viewport_top_left(cmd, extent);

    if (!push_ortho_mvp) {
        return;
    }

    PushConstants pc{};
    Mat4 const ortho =
        mat4_ortho_top_left(static_cast<float>(extent.width), static_cast<float>(extent.height));
    mat4_to_glsl(ortho, pc.mvp);
    vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
}

void Draw2dBatch::reset() noexcept {
    colored_vertices_.clear();
    ops_.clear();
    bg_colored_vertices_.clear();
    bg_ops_.clear();
    background_layer_ = false;
}

void Draw2dBatch::begin_background_layer() noexcept {
    background_layer_ = true;
}

void Draw2dBatch::end_background_layer() noexcept {
    background_layer_ = false;
}

void Draw2dBatch::set_screen_size(int width, int height) noexcept {
    screen_w_ = std::max(1, width);
    screen_h_ = std::max(1, height);
}

ColoredVertex Draw2dBatch::make_vertex(float x, float y, Color c) noexcept {
    return ColoredVertex{x, y, 0.f, static_cast<float>(c.r) / 255.f, static_cast<float>(c.g) / 255.f,
                         static_cast<float>(c.b) / 255.f, static_cast<float>(c.a) / 255.f};
}

void Draw2dBatch::append_colored_cmd() {
    auto &ops = background_layer_ ? bg_ops_ : ops_;
    if (!ops.empty()) {
        return;
    }
    auto &verts = background_layer_ ? bg_colored_vertices_ : colored_vertices_;
    ops.push_back(ColoredDrawCmd{static_cast<std::uint32_t>(verts.size()), 0});
}

void Draw2dBatch::push_vertex(float x, float y, Color color) {
    append_colored_cmd();
    auto &verts = background_layer_ ? bg_colored_vertices_ : colored_vertices_;
    if (verts.size() >= kMaxVertices - 3) {
        return;
    }
    verts.push_back(make_vertex(x, y, color));
}

void Draw2dBatch::push_triangle(ColoredVertex const &a, ColoredVertex const &b,
                                ColoredVertex const &c) {
    append_colored_cmd();
    auto &verts = background_layer_ ? bg_colored_vertices_ : colored_vertices_;
    auto &ops = background_layer_ ? bg_ops_ : ops_;
    if (verts.size() + 3 > kMaxVertices) {
        return;
    }
    verts.push_back(a);
    verts.push_back(b);
    verts.push_back(c);
    auto &cmd = ops.back();
    cmd.vertex_count += 3;
}

void Draw2dBatch::fill_rect(float x, float y, float w, float h, Color color) {
    if (w <= 0.f || h <= 0.f) {
        return;
    }
    auto v0 = make_vertex(x, y, color);
    auto v1 = make_vertex(x + w, y, color);
    auto v2 = make_vertex(x + w, y + h, color);
    auto v3 = make_vertex(x, y + h, color);
    push_triangle(v0, v1, v2);
    push_triangle(v0, v2, v3);
}

void Draw2dBatch::stroke_rect(float x, float y, float w, float h, float thickness, Color color) {
    if (thickness <= 0.f) {
        return;
    }
    fill_rect(x, y, w, thickness, color);
    fill_rect(x, y + h - thickness, w, thickness, color);
    fill_rect(x, y, thickness, h, color);
    fill_rect(x + w - thickness, y, thickness, h, color);
}

void Draw2dBatch::line(Vector2 from, Vector2 to, float thickness, Color color) {
    if (thickness <= 0.f) {
        return;
    }
    float const dx = to.x - from.x;
    float const dy = to.y - from.y;
    float const len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) {
        return;
    }
    float const nx = -dy / len * thickness * 0.5f;
    float const ny = dx / len * thickness * 0.5f;
    push_triangle(make_vertex(from.x + nx, from.y + ny, color),
                  make_vertex(to.x + nx, to.y + ny, color),
                  make_vertex(to.x - nx, to.y - ny, color));
    push_triangle(make_vertex(from.x + nx, from.y + ny, color),
                  make_vertex(to.x - nx, to.y - ny, color),
                  make_vertex(from.x - nx, from.y - ny, color));
}

void Draw2dBatch::circle(Vector2 center, float radius, Color color, int segments) {
    segments = std::max(3, segments);
    float const step = 6.28318530718f / static_cast<float>(segments);
    auto prev = make_vertex(center.x + radius, center.y, color);
    for (int i = 1; i <= segments; ++i) {
        float const a = step * static_cast<float>(i);
        auto cur = make_vertex(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius,
                               color);
        push_triangle(make_vertex(center.x, center.y, color), prev, cur);
        prev = cur;
    }
}

void Draw2dBatch::circle_outline(Vector2 center, float radius, Color color, int segments) {
    segments = std::max(3, segments);
    float const step = 6.28318530718f / static_cast<float>(segments);
    Vector2 prev{center.x + radius, center.y};
    for (int i = 1; i <= segments; ++i) {
        float const a = step * static_cast<float>(i);
        Vector2 cur{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
        line(prev, cur, 1.5f, color);
        prev = cur;
    }
}

void Draw2dBatch::ring(Vector2 center, float inner, float outer, float start_deg, float end_deg,
                       int segments, Color color) {
    if (outer <= inner || segments < 3) {
        return;
    }
    float start = start_deg * 0.01745329252f;
    float end = end_deg * 0.01745329252f;
    if (end < start) {
        end += 6.28318530718f;
    }
    float const step = (end - start) / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a0 = start + step * static_cast<float>(i);
        float a1 = start + step * static_cast<float>(i + 1);
        float c0 = std::cos(a0);
        float s0 = std::sin(a0);
        float c1 = std::cos(a1);
        float s1 = std::sin(a1);
        auto v0 = make_vertex(center.x + c0 * inner, center.y + s0 * inner, color);
        auto v1 = make_vertex(center.x + c1 * inner, center.y + s1 * inner, color);
        auto v2 = make_vertex(center.x + c1 * outer, center.y + s1 * outer, color);
        auto v3 = make_vertex(center.x + c0 * outer, center.y + s0 * outer, color);
        push_triangle(v0, v1, v2);
        push_triangle(v0, v2, v3);
    }
}

bool Draw2dBatch::create_gpu(VkContext const &ctx) {
    auto create_buffer = [&](VkDeviceSize size, VkBuffer &buffer, VkDeviceMemory &memory,
                             void **mapped) {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(ctx.device(), &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
            return false;
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(ctx.device(), buffer, &req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = find_memory_type(
            ctx.physical_device(), req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(ctx.device(), &alloc, nullptr, &memory) != VK_SUCCESS) {
            return false;
        }
        vkBindBufferMemory(ctx.device(), buffer, memory, 0);
        return vkMapMemory(ctx.device(), memory, 0, size, 0, mapped) == VK_SUCCESS;
    };

    VkDeviceSize const colored_size = sizeof(ColoredVertex) * kMaxVertices;
    return create_buffer(colored_size, colored_vertex_buffer_, colored_vertex_memory_,
                         &colored_mapped_);
}

void Draw2dBatch::destroy_gpu(VkContext const &ctx) {
    if (colored_mapped_ != nullptr) {
        vkUnmapMemory(ctx.device(), colored_vertex_memory_);
        colored_mapped_ = nullptr;
    }
    if (colored_vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx.device(), colored_vertex_buffer_, nullptr);
        colored_vertex_buffer_ = VK_NULL_HANDLE;
    }
    if (colored_vertex_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device(), colored_vertex_memory_, nullptr);
        colored_vertex_memory_ = VK_NULL_HANDLE;
    }
}

namespace {

void flush_colored_ops(VkCommandBuffer cmd, VkColoredPipeline const &colored_pipe, VkExtent2D extent,
                       VkBuffer vertex_buffer, void *mapped,
                       std::vector<ColoredVertex> const &vertices,
                       std::vector<ColoredDrawCmd> const &ops) {
    if (ops.empty() || vertices.empty() || mapped == nullptr) {
        return;
    }

    std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(ColoredVertex));

    colored_pipe.bind_and_set_viewport(cmd, extent);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);

    for (ColoredDrawCmd const &draw : ops) {
        if (draw.vertex_count == 0) {
            continue;
        }
        vkCmdDraw(cmd, draw.vertex_count, 1, draw.first_vertex, 0);
    }
}

}  // namespace

void Draw2dBatch::flush_background(VkCommandBuffer cmd, VkColoredPipeline const &colored_pipe,
                                   VkExtent2D extent) {
    flush_colored_ops(cmd, colored_pipe, extent, colored_vertex_buffer_, colored_mapped_,
                      bg_colored_vertices_, bg_ops_);
}

void Draw2dBatch::flush(VkCommandBuffer cmd, VkColoredPipeline const &colored_pipe,
                        VkExtent2D extent) {
    flush_colored_ops(cmd, colored_pipe, extent, colored_vertex_buffer_, colored_mapped_,
                      colored_vertices_, ops_);
}

}  // namespace zh::gfx::vk
