#pragma once

#include <vector>

namespace zh::gfx::vk {

// Load a font file into SFNT bytes (TTF/OTF as-is; WOFF2 decoded when linked).
[[nodiscard]] bool load_font_bytes(char const *path, std::vector<unsigned char> &out);

}  // namespace zh::gfx::vk
