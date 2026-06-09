#pragma once

// Screen-ray tests against the lit 3D playing scene (debug picking).

#include "zh/ui/playing_scene_3d.hpp"

#include "zh/gfx/gfx_compat.hpp"

namespace zh::ui {

enum class PlayingPickKind : std::uint8_t {
    None,
    IcePlane,
    IceSlab,
    BoardWall,
    Puck,
    SkaterHost,
    SkaterSlot,
    WaypointHost,
    WaypointSlot,
    ShieldHost,
    ShieldSlot,
};

struct PlayingPickHit {
    PlayingPickKind kind{PlayingPickKind::None};
    float distance{0.f};
    Vector3 point{};
    Vector2 sim{};
    int slot_index{-1};
    char label[40]{};
};

struct PlayingPickResult {
    Ray ray{};
    Vector2 screen{};
    bool ice_plane_ok{false};
    Vector2 ice_plane_sim{};
    float ice_plane_err_px{-1.f};
    int hit_count{0};
    static constexpr int kMaxHits = 16;
    PlayingPickHit hits[kMaxHits]{};
    PlayingPickHit closest{};
};

[[nodiscard]] PlayingPickResult pick_playing_scene(Vector2 screen,
                                                   Camera3D const &cam,
                                                   PlayingWorldDraw const &world) noexcept;

void log_playing_pick_to_stderr(char const *button_tag,
                                PlayingPickResult const &pick,
                                Vector2 screen,
                                Vector2 ice_sim,
                                bool ice_ok,
                                float sent_sim_x,
                                float sent_sim_y) noexcept;

// F3 overlay: screen-space ray/error lines plus 3D markers on the ice plane.
void draw_playing_pick_debug(Camera3D const &cam,
                             Vector2 screen,
                             Vector2 clamped_sim,
                             bool pick_ok,
                             float sent_sim_x,
                             float sent_sim_y,
                             PlayingPickResult const &pick) noexcept;

}  // namespace zh::ui
