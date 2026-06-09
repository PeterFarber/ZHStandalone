#pragma once

// Rectangle hit test helper for UI widgets.

#include "zh/gfx/gfx_compat.hpp"

namespace zh::ui {

[[nodiscard]] inline bool point_in(Rectangle rect, Vector2 pt) noexcept {
    return CheckCollisionPointRec(pt, rect);
}

}  // namespace zh::ui
