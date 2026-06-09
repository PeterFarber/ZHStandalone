#pragma once

// 3D playing view: map + mesh entities in BeginMode3D; HUD/name tags stay 2D overlays.

#include "zh/protocol.hpp"

#include "zh/gfx/gfx_compat.hpp"

#include <array>

namespace zh::ui {

struct PlayingEntityDraw {
    bool active{false};
    float x{0.f};
    float y{0.f};
    bool team_b{false};
    bool goalie{false};
    bool is_local{false};
    bool has_waypoint{false};
    float way_x{0.f};
    float way_y{0.f};
    float one_timer_frac{0.f};
    float shield_frac{0.f};
    char name[24]{};
};

struct PlayingWorldDraw {
    bool active{false};
    float puck_x{0.f};
    float puck_y{0.f};
    PlayingEntityDraw host{};
    std::array<PlayingEntityDraw, zh::kRemoteSlots> slots{};
};

void draw_puck_3d(float puck_x, float puck_y) noexcept;
void draw_skater_3d_entity(PlayingEntityDraw const &entity) noexcept;
void draw_waypoint_marker_3d(float wx, float wy) noexcept;
void draw_shield_ring_3d(float sx, float sy, float frac) noexcept;

// Static demo layout for the render-test scene (host at spawn, puck at center).
[[nodiscard]] PlayingWorldDraw make_render_test_demo_world(float host_x,
                                                           float host_y) noexcept;

class SkaterFxSystem;

class PlayingScene3D {
public:
    void ensure_ready() noexcept;
    void unload() noexcept;

    void draw(Camera3D const &cam, PlayingWorldDraw const &world,
              SkaterFxSystem const *fx = nullptr) const noexcept;
    void draw_overlays(Camera3D const &cam, PlayingWorldDraw const &world) const noexcept;

    // Same stack as render-test step 11: lit 3D world + screen-space gameplay overlays.
    void draw_frame(Camera3D const &cam, PlayingWorldDraw const &world,
                    SkaterFxSystem const *fx = nullptr) const noexcept;
};

}  // namespace zh::ui
