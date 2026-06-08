#pragma once

// MapDefinition JSON load/save (maps/*.json). load_map_file merges file fields onto baked default.

#include "zh/game/map_definition.hpp"

#include <string>

namespace zh::game {

[[nodiscard]] std::string default_map_path() noexcept;
[[nodiscard]] bool load_map_file(char const *path, MapDefinition &out) noexcept;
[[nodiscard]] bool save_map_file(char const *path, MapDefinition const &def) noexcept;

}  // namespace zh::game
