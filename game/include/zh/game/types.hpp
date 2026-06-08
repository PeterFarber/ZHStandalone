#pragma once

// Shared layout types (no Raylib).

namespace zh::game {

// Axis-aligned rectangle in rink / UI layout space (no raylib dependency).
struct RectF {
    float x{};
    float y{};
    float w{};
    float h{};
};

}  // namespace zh::game
