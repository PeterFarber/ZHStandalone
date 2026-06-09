#pragma once

#include "zh/gfx/gfx_types.hpp"
#include "zh/gfx/vulkan/vk_context.hpp"

namespace zh::gfx::vk {

class Draw2dBatch;

class VkFont {
public:
    bool init(VkContext const &ctx);
    void shutdown(VkContext const &ctx);

    void draw_text(Draw2dBatch &batch, char const *text, int pos_x, int pos_y, int font_size,
                   Color color) const;
    [[nodiscard]] int measure_text(char const *text, int font_size) const;

private:
    struct State;
    State *state_{nullptr};
};

}  // namespace zh::gfx::vk
