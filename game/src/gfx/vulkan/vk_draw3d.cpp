#include "zh/gfx/vulkan/vk_draw3d.hpp"

#include "zh/gfx/gfx_constants.hpp"
#include "zh/gfx/vulkan/vk_matrix.hpp"
#include "zh/gfx/vulkan/vk_util.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <numbers>
#include <stdexcept>

namespace zh::gfx::vk {

namespace {

struct alignas(16) PushConstants {
    float proj[16];
    float view[16];
};

static_assert(sizeof(PushConstants) == 128U);

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

[[nodiscard]] Vector3 face_normal(Vector3 a, Vector3 b, Vector3 c) noexcept {
    float const ux = b.x - a.x;
    float const uy = b.y - a.y;
    float const uz = b.z - a.z;
    float const vx = c.x - a.x;
    float const vy = c.y - a.y;
    float const vz = c.z - a.z;
    Vector3 n{uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx};
    float const len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len <= 1e-6f) {
        return Vector3{0.f, 1.f, 0.f};
    }
    float const inv = 1.f / len;
    return Vector3{n.x * inv, n.y * inv, n.z * inv};
}

[[nodiscard]] std::uint32_t find_memory_type(VkPhysicalDevice device, std::uint32_t type_bits,
                                             VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(device, &mem_props);
    for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_bits & (1U << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) ==
                                           properties) {
            return i;
        }
    }
    throw std::runtime_error("find_memory_type failed");
}

}  // namespace

bool VkLitColoredPipeline::load_shaders(VkContext const &ctx, VkShaderModule &vert,
                                        VkShaderModule &frag) const {
    auto const vert_bytes = read_binary_file(resolve_shader_spv("colored_lit.vert.spv"));
    auto const frag_bytes = read_binary_file(resolve_shader_spv("colored_lit.frag.spv"));
    vert = create_shader_module(ctx.device(), vert_bytes);
    frag = create_shader_module(ctx.device(), frag_bytes);
    return vert != VK_NULL_HANDLE && frag != VK_NULL_HANDLE;
}

bool VkLitColoredPipeline::create(VkContext const &ctx, VkSwapchain const &swapchain) {
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
    binding.stride = sizeof(LitColoredVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(LitColoredVertex, x);
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(LitColoredVertex, nx);
    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(LitColoredVertex, r);

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 3;
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
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
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

    bool const ok = vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &pipe_info, nullptr,
                                              &pipeline_) == VK_SUCCESS;
    destroy_shader_module(ctx.device(), vert);
    destroy_shader_module(ctx.device(), frag);
    return ok;
}

void VkLitColoredPipeline::destroy(VkContext const &ctx) {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx.device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx.device(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
}

void VkLitColoredPipeline::bind_and_set_viewport(VkCommandBuffer cmd, VkExtent2D extent) const {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    set_viewport_top_left(cmd, extent);
}

void Draw3dBatch::reset() noexcept {
    vertices_.clear();
    camera_ready_ = false;
}

void Draw3dBatch::begin(Camera3D camera) noexcept {
    camera_ = camera;
    camera_ready_ = true;
}

void Draw3dBatch::end() noexcept {}

LitColoredVertex Draw3dBatch::make_vertex(float x, float y, float z, float nx, float ny,
                                           float nz, Color c) noexcept {
    return LitColoredVertex{x,
                            y,
                            z,
                            nx,
                            ny,
                            nz,
                            static_cast<float>(c.r) / 255.f,
                            static_cast<float>(c.g) / 255.f,
                            static_cast<float>(c.b) / 255.f,
                            static_cast<float>(c.a) / 255.f};
}

void Draw3dBatch::push_triangle(LitColoredVertex const &a, LitColoredVertex const &b,
                                LitColoredVertex const &c) {
    if (vertices_.size() + 3 > kMaxVertices) {
        return;
    }
    vertices_.push_back(a);
    vertices_.push_back(b);
    vertices_.push_back(c);
}

void Draw3dBatch::triangle(Vector3 a, Vector3 b, Vector3 c, Color color) {
    if (!camera_ready_) {
        return;
    }
    Vector3 const n = face_normal(a, b, c);
    push_triangle(make_vertex(a.x, a.y, a.z, n.x, n.y, n.z, color),
                  make_vertex(b.x, b.y, b.z, n.x, n.y, n.z, color),
                  make_vertex(c.x, c.y, c.z, n.x, n.y, n.z, color));
}

void Draw3dBatch::cylinder(Vector3 center, float radius_top, float radius_bottom, float height,
                           int slices, Color color) {
    if (!camera_ready_ || height <= 0.f || slices < 3) {
        return;
    }
    slices = std::clamp(slices, 3, 64);
    float const y0 = center.y - height * 0.5f;
    float const y1 = center.y + height * 0.5f;
    float const tau = 2.f * std::numbers::pi_v<float>;

    auto ring_point = [&](float y, float radius, float angle) {
        return Vector3{center.x + std::cos(angle) * radius, y, center.z + std::sin(angle) * radius};
    };

    for (int i = 0; i < slices; ++i) {
        float const a0 = (static_cast<float>(i) / static_cast<float>(slices)) * tau;
        float const a1 = (static_cast<float>(i + 1) / static_cast<float>(slices)) * tau;
        float const c0 = std::cos(a0);
        float const s0 = std::sin(a0);
        float const c1 = std::cos(a1);
        float const s1 = std::sin(a1);

        Vector3 const t0 = ring_point(y1, radius_top, a0);
        Vector3 const t1 = ring_point(y1, radius_top, a1);
        Vector3 const c_top{center.x, y1, center.z};
        push_triangle(make_vertex(c_top.x, c_top.y, c_top.z, 0.f, 1.f, 0.f, color),
                      make_vertex(t0.x, t0.y, t0.z, c0, 0.f, s0, color),
                      make_vertex(t1.x, t1.y, t1.z, c1, 0.f, s1, color));

        Vector3 const b0 = ring_point(y0, radius_bottom, a0);
        Vector3 const b1 = ring_point(y0, radius_bottom, a1);
        Vector3 const c_bot{center.x, y0, center.z};
        push_triangle(make_vertex(c_bot.x, c_bot.y, c_bot.z, 0.f, -1.f, 0.f, color),
                      make_vertex(b1.x, b1.y, b1.z, c1, 0.f, s1, color),
                      make_vertex(b0.x, b0.y, b0.z, c0, 0.f, s0, color));

        push_triangle(make_vertex(b0.x, b0.y, b0.z, c0, 0.f, s0, color),
                      make_vertex(t0.x, t0.y, t0.z, c0, 0.f, s0, color),
                      make_vertex(t1.x, t1.y, t1.z, c1, 0.f, s1, color));
        push_triangle(make_vertex(b0.x, b0.y, b0.z, c0, 0.f, s0, color),
                      make_vertex(t1.x, t1.y, t1.z, c1, 0.f, s1, color),
                      make_vertex(b1.x, b1.y, b1.z, c1, 0.f, s1, color));
    }
}

void Draw3dBatch::cube(Vector3 center, float width, float height, float depth, Color color) {
    if (!camera_ready_ || width <= 0.f || height <= 0.f || depth <= 0.f) {
        return;
    }
    float const hx = width * 0.5f;
    float const hy = height * 0.5f;
    float const hz = depth * 0.5f;

    Vector3 const p000{center.x - hx, center.y - hy, center.z - hz};
    Vector3 const p001{center.x - hx, center.y - hy, center.z + hz};
    Vector3 const p010{center.x - hx, center.y + hy, center.z - hz};
    Vector3 const p011{center.x - hx, center.y + hy, center.z + hz};
    Vector3 const p100{center.x + hx, center.y - hy, center.z - hz};
    Vector3 const p101{center.x + hx, center.y - hy, center.z + hz};
    Vector3 const p110{center.x + hx, center.y + hy, center.z - hz};
    Vector3 const p111{center.x + hx, center.y + hy, center.z + hz};

    auto quad = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 n) {
        push_triangle(make_vertex(a.x, a.y, a.z, n.x, n.y, n.z, color),
                      make_vertex(b.x, b.y, b.z, n.x, n.y, n.z, color),
                      make_vertex(c.x, c.y, c.z, n.x, n.y, n.z, color));
        push_triangle(make_vertex(a.x, a.y, a.z, n.x, n.y, n.z, color),
                      make_vertex(c.x, c.y, c.z, n.x, n.y, n.z, color),
                      make_vertex(d.x, d.y, d.z, n.x, n.y, n.z, color));
    };

    quad(p010, p011, p111, p110, Vector3{0.f, 1.f, 0.f});
    quad(p000, p100, p101, p001, Vector3{0.f, -1.f, 0.f});
    quad(p000, p001, p011, p010, Vector3{-1.f, 0.f, 0.f});
    quad(p100, p110, p111, p101, Vector3{1.f, 0.f, 0.f});
    quad(p001, p101, p111, p011, Vector3{0.f, 0.f, 1.f});
    quad(p000, p010, p110, p100, Vector3{0.f, 0.f, -1.f});
}

bool Draw3dBatch::create_gpu(VkContext const &ctx) {
    VkDeviceSize const size = sizeof(LitColoredVertex) * kMaxVertices;
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(ctx.device(), &buffer_info, nullptr, &vertex_buffer_) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_req{};
    vkGetBufferMemoryRequirements(ctx.device(), vertex_buffer_, &mem_req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = mem_req.size;
    alloc.memoryTypeIndex = find_memory_type(ctx.physical_device(), mem_req.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(ctx.device(), &alloc, nullptr, &vertex_memory_) != VK_SUCCESS) {
        vkDestroyBuffer(ctx.device(), vertex_buffer_, nullptr);
        vertex_buffer_ = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(ctx.device(), vertex_buffer_, vertex_memory_, 0);
    vkMapMemory(ctx.device(), vertex_memory_, 0, size, 0, &mapped_);
    return mapped_ != nullptr;
}

void Draw3dBatch::destroy_gpu(VkContext const &ctx) {
    if (mapped_ != nullptr) {
        vkUnmapMemory(ctx.device(), vertex_memory_);
        mapped_ = nullptr;
    }
    if (vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx.device(), vertex_buffer_, nullptr);
        vertex_buffer_ = VK_NULL_HANDLE;
    }
    if (vertex_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.device(), vertex_memory_, nullptr);
        vertex_memory_ = VK_NULL_HANDLE;
    }
}

void Draw3dBatch::flush(VkCommandBuffer cmd, VkContext const &ctx,
                        VkLitColoredPipeline const &pipeline, VkExtent2D extent) {
    (void)ctx;
    if (vertices_.empty() || !camera_ready_ || mapped_ == nullptr) {
        return;
    }

    std::memcpy(mapped_, vertices_.data(), vertices_.size() * sizeof(LitColoredVertex));

    pipeline.bind_and_set_viewport(cmd, extent);

    PushConstants pc{};
    float const aspect =
        static_cast<float>(extent.width) / static_cast<float>(extent.height > 0 ? extent.height : 1);
    Mat4 const view = mat4_look_at(camera_.position, camera_.target, camera_.up);
    Mat4 const proj = mat4_perspective_vulkan(camera_.fovy * DEG2RAD, aspect, kCameraNearPlane,
                                              kCameraFarPlane);
    mat4_to_glsl(proj, pc.proj);
    mat4_to_glsl(view, pc.view);
    vkCmdPushConstants(cmd, pipeline.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants),
                       &pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer_, &offset);
    vkCmdDraw(cmd, static_cast<std::uint32_t>(vertices_.size()), 1, 0, 0);
}

}  // namespace zh::gfx::vk
