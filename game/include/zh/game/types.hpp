#pragma once

// Shared layout types (no gfx dependencies).

namespace zh::game {

// Axis-aligned rectangle in rink / UI layout space.
struct RectF {
    float x{};
    float y{};
    float w{};
    float h{};
};

}  // namespace zh::game
