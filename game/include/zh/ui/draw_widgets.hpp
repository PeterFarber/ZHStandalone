#pragma once

// Shared UI widgets: buttons and form controls.

#include "zh/gfx/gfx_compat.hpp"

namespace zh::ui {

void draw_centered_rect_button(Rectangle r,
                               Color fill,
                               Color border,
                               char const *label,
                               int font_px,
                               bool hovered) noexcept;

}  // namespace zh::ui
