#include "zh/ui/playing_scene_3d.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/ui/map_shape_draw.hpp"
#include "zh/ui/playing_hud.hpp"
#include "zh/ui/skater_fx.hpp"
#include "zh/ui/world_space_3d.hpp"

#include <algorithm>

namespace zh::ui {

namespace {

using namespace zh::game;

inline constexpr float kPuckDrawHeight = 3.5f;

[[nodiscard]] Color team_fill(bool team_b) noexcept {
    return team_b ? ColorFromHSV(6.f, 0.74f, 0.93f) : ColorFromHSV(200.f, 0.74f, 0.93f);
}

[[nodiscard]] Color team_name_color(bool team_b) noexcept {
    return team_b ? Color{220, 70, 70, 255} : Color{70, 150, 235, 255};
}

void draw_timer_bar_screen(Vector2 screen, float frac) noexcept {
    constexpr float bar_w = 64.f;
    constexpr float bar_h = 7.f;
    float const x = screen.x - bar_w * 0.5f;
    float const y = screen.y - 54.f;
    DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(bar_w),
                  static_cast<int>(bar_h), Color{16, 24, 32, 210});
    frac = std::clamp(frac, 0.f, 1.f);
    if (frac > 0.f) {
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(bar_w * frac),
                      static_cast<int>(bar_h), Color{90, 220, 255, 255});
    }
    DrawRectangleLinesEx(Rectangle{x, y, bar_w, bar_h}, 1.5f, Color{180, 230, 255, 220});
}

}  // namespace

void draw_puck_3d(float const puck_x, float const puck_y) noexcept {
    Vector3 const base = sim_to_world3(puck_x, puck_y, kPuckDrawHeight * 0.5f);
    DrawCylinder(base, kPuckDrawRadius, kPuckDrawRadius, kPuckDrawHeight, 20,
                 Color{30, 32, 38, 255});
}

void draw_skater_3d_entity(PlayingEntityDraw const &entity) noexcept {
    if (!entity.active) {
        return;
    }
    float const radius = player_draw_radius(entity.goalie) * 1.35f;
    float const height = (entity.goalie ? kGoalieBodyHeight : kSkaterBodyHeight) * 1.2f;
    Color fill = team_fill(entity.team_b);
    Color rim = team_fill(entity.team_b);
    if (entity.is_local) {
        fill = ColorBrightness(fill, 0.12f);
        rim = ColorBrightness(rim, 0.32f);
    }

    Vector3 const body_center = sim_to_world3(entity.x, entity.y, height * 0.5f);
    DrawCylinder(body_center, radius, radius, height, 28, fill);

    Vector3 const rim_center = sim_to_world3(entity.x, entity.y, 1.8f);
    DrawCylinder(rim_center, radius + 1.2f, radius + 1.2f, 3.6f, 28, rim);
}

void draw_waypoint_marker_3d(float const wx, float const wy) noexcept {
    Vector3 const base = sim_to_world3(wx, wy, kPlayingWaypointY);
    DrawCylinder(base, 7.f, 7.f, kPlayingWaypointDiscHeight, 16, Color{255, 220, 100, 220});
}

void draw_shield_ring_3d(float const sx, float const sy, float frac) noexcept {
    frac = std::clamp(frac, 0.f, 1.f);
    unsigned char const alpha =
        static_cast<unsigned char>(std::clamp(70.f + 150.f * frac, 40.f, 230.f));
    Vector3 const center = sim_to_world3(sx, sy, 2.f);
    DrawCylinder(center, kGoalieShieldRadius, kGoalieShieldRadius, 4.f, 32,
                 Color{120, 210, 255, alpha});
}

PlayingWorldDraw make_render_test_demo_world(float const host_x, float const host_y) noexcept {
    PlayingWorldDraw world{};
    world.active = true;
    world.puck_x = rink_center_x();
    world.puck_y = rink_mid_y();

    world.host.active = true;
    world.host.x = host_x;
    world.host.y = host_y;
    world.host.is_local = true;
    world.host.has_waypoint = true;
    world.host.way_x = host_x + 90.f;
    world.host.way_y = host_y + 50.f;
    world.host.shield_frac = 0.65f;

    world.slots[0].active = true;
    world.slots[0].x = host_x + 140.f;
    world.slots[0].y = host_y - 30.f;
    world.slots[0].team_b = true;
    return world;
}

void PlayingScene3D::ensure_ready() noexcept {}

void PlayingScene3D::unload() noexcept {}

void PlayingScene3D::draw(Camera3D const &cam, PlayingWorldDraw const &world,
                          SkaterFxSystem const *fx) const noexcept {
    if (!world.active) {
        return;
    }

    MapDefinition const &map = map_runtime().definition();

    BeginMode3D(cam);

    // Rink shell: stadium, surround + ice, board walls, goals.
    draw_map_playfield_3d(map);
    // Gameplay bodies.
    draw_puck_3d(world.puck_x, world.puck_y);
    if (world.host.active) {
        draw_skater_3d_entity(world.host);
    }
    for (PlayingEntityDraw const &slot : world.slots) {
        draw_skater_3d_entity(slot);
    }
    // Aim / shield FX.
    if (world.host.active) {
        if (world.host.has_waypoint) {
            draw_waypoint_marker_3d(world.host.way_x, world.host.way_y);
        }
        if (world.host.shield_frac > 0.f) {
            draw_shield_ring_3d(world.host.x, world.host.y, world.host.shield_frac);
        }
    }
    for (PlayingEntityDraw const &slot : world.slots) {
        if (!slot.active) {
            continue;
        }
        if (slot.has_waypoint) {
            draw_waypoint_marker_3d(slot.way_x, slot.way_y);
        }
        if (slot.shield_frac > 0.f) {
            draw_shield_ring_3d(slot.x, slot.y, slot.shield_frac);
        }
    }
    // Rink markings on the ice surface (after bodies so line art stays visible).
    draw_map_rink_strokes_3d(map);

    if (fx != nullptr) {
        fx->draw_3d();
    }

    EndMode3D();
}

void PlayingScene3D::draw_overlays(Camera3D const &cam,
                                   PlayingWorldDraw const &world) const noexcept {
    if (!world.active) {
        return;
    }

    auto bar_at = [&](float sx, float sy, float frac) {
        Vector2 screen{};
        if (!TryGetWorldToScreen(sim_to_world3(sx, sy, player_draw_radius(false) + 8.f), cam,
                                 screen)) {
            return;
        }
        draw_timer_bar_screen(screen, frac);
    };

    if (world.host.active && world.host.one_timer_frac > 0.f) {
        bar_at(world.host.x, world.host.y, world.host.one_timer_frac);
    }

    for (PlayingEntityDraw const &slot : world.slots) {
        if (!slot.active) {
            continue;
        }
        if (slot.one_timer_frac > 0.f) {
            bar_at(slot.x, slot.y, slot.one_timer_frac);
        }
    }

    auto name_at = [&](PlayingEntityDraw const &entity) {
        if (entity.name[0] == '\0') {
            return;
        }
        Vector2 screen{};
        if (!TryGetWorldToScreen(
                sim_to_world3(entity.x, entity.y, player_draw_radius(entity.goalie) + 22.f), cam,
                screen)) {
            return;
        }
        draw_player_name_tag(screen, entity.name, team_name_color(entity.team_b));
    };

    if (world.host.active) {
        name_at(world.host);
    }
    for (PlayingEntityDraw const &slot : world.slots) {
        if (slot.active) {
            name_at(slot);
        }
    }
}

void PlayingScene3D::draw_frame(Camera3D const &cam, PlayingWorldDraw const &world,
                                SkaterFxSystem const *fx) const noexcept {
    draw(cam, world, fx);
    draw_overlays(cam, world);
}

}  // namespace zh::ui
