#pragma once

// Modular rink art load/draw from resources/rink/*.png.

#include "zh/game/map_definition.hpp"

#include <raylib.h>

namespace zh::ui {

struct RinkSpriteSet {
    Texture2D walls{};
    Texture2D circle{};
    Texture2D goal{};

    bool ready{false};  // walls, circle, and goal PNGs loaded from resources/rink/
};

void load_rink_sprites(RinkSpriteSet &out) noexcept;
void unload_rink_sprites(RinkSpriteSet &sprites) noexcept;

void draw_modular_rink(RinkSpriteSet const &sprites) noexcept;

}  // namespace zh::ui
