#pragma once

#include "zh/gfx/gfx_types.hpp"

namespace zh::gfx {

bool init(int width, int height, char const *title);
void shutdown();

void begin_drawing();
void end_drawing();
void clear_background(Color color);

}  // namespace zh::gfx
