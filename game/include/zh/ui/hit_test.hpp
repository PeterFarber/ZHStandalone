#pragma once

// Rectangle hit test helper for UI widgets.

#include <raylib.h>

namespace zh::ui {

[[nodiscard]] inline bool point_in(Rectangle rect, Vector2 pt) noexcept {
    return CheckCollisionPointRec(pt, rect);
}

}  // namespace zh::ui
