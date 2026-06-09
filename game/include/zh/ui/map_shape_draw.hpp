#pragma once

// MapDefinition background + editor shapes (no PNG rink art).

#include "zh/game/map_definition.hpp"

#include "zh/gfx/gfx_compat.hpp"

namespace zh::ui {

[[nodiscard]] Color parse_map_css_color(std::string const &text) noexcept;

// Incremental playfield build-out (render test scene). Order matches playing draw stack.
enum class PlayfieldBuildStep : std::uint8_t {
    Origin = 0,
    IceSurface,
    Surround,
    StadiumBackdrop,
    BoardWalls,
    Goals,
    RinkMarkings,
    PlayerProxy,
    Puck,
    OpponentSkater,
    GameplayFx,
    FullPlayingPath,
    Count,
};

[[nodiscard]] char const *playfield_build_step_name(PlayfieldBuildStep step) noexcept;

[[nodiscard]] inline int playfield_build_step_count() noexcept {
    return static_cast<int>(PlayfieldBuildStep::Count);
}

void draw_playfield_build_step_3d(zh::game::MapDefinition const &map,
                                  PlayfieldBuildStep step) noexcept;

void draw_map_playfield_3d(zh::game::MapDefinition const &map) noexcept;

void draw_map_rink_strokes_3d(zh::game::MapDefinition const &map) noexcept;

}  // namespace zh::ui
