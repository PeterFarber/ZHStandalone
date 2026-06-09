// Shared UI widgets: buttons and form controls.

#include "zh/ui/draw_widgets.hpp"

namespace zh::ui {

void draw_centered_rect_button(Rectangle const r,
                               Color const fill,
                               Color const border,
                               char const *const label,
                               int const font_px,
                               bool const hovered) noexcept {
    Color const fill_draw = hovered ? ColorBrightness(fill, 0.22f) : fill;
    Color const border_draw = hovered ? ColorBrightness(border, 0.42f) : border;
    float const thick = hovered ? 4.5f : 3.f;
    DrawRectangleRec(r, fill_draw);
    DrawRectangleLinesEx(r, thick, border_draw);
    Color const label_color = hovered ? OFF_WHITE : ColorBrightness(OFF_WHITE, -0.08f);
    int const lw = MeasureText(label, font_px);
    float const lx =
        r.x + (static_cast<float>(r.width) - static_cast<float>(lw)) * 0.5f;
    float const ly =
        r.y +
        (static_cast<float>(r.height) - static_cast<float>(font_px)) * 0.5f;
    DrawText(label, static_cast<int>(lx), static_cast<int>(ly), font_px, label_color);
}

}  // namespace zh::ui
