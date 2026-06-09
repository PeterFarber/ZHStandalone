#include "zh/ui/render_test_scene.hpp"

#include "zh/ui/map_shape_draw.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/playing_camera.hpp"
#include "zh/game/playing_camera_3d.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/game/skater_physics.hpp"
#include "zh/ui/world_space_3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zh::ui {

namespace {

using namespace zh::game;

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kDegToRad = kPi / 180.f;
inline constexpr float kFocusPanSpeed = 420.f;
inline constexpr float kMinViewFraction = 1e-5f;
inline constexpr float kFlyMinHeightY = 24.f;
inline constexpr float kFlyMaxHeightY = 8000.f;

[[nodiscard]] Camera3D make_render_test_playing_camera(float focus_x,
                                                       float focus_y,
                                                       float view_fraction,
                                                       float viewport_w,
                                                       float viewport_h,
                                                       RectF const &play_bounds) noexcept {
    return make_playing_camera_3d(focus_x, focus_y, std::max(view_fraction, kMinViewFraction),
                                  viewport_w, viewport_h, play_bounds, false);
}

[[nodiscard]] char const *step_hint(PlayfieldBuildStep const step) noexcept {
    switch (step) {
        case PlayfieldBuildStep::Origin:
            return "Spawn axes: sim X=red, world Y=green, sim Y=blue.";
        case PlayfieldBuildStep::IceSurface:
            return "Ice slab only (player_bounds rect).";
        case PlayfieldBuildStep::Surround:
            return "Full camera-bounds floor below ice; darker border shows at edges when zoomed out.";
        case PlayfieldBuildStep::StadiumBackdrop:
            return "Stadium backdrop shell behind the rink.";
        case PlayfieldBuildStep::BoardWalls:
            return "Extruded board walls from map outline.";
        case PlayfieldBuildStep::Goals:
            return "Goal frames and nets.";
        case PlayfieldBuildStep::RinkMarkings:
            return "Extruded rink markings on the ice surface (3D strokes).";
        case PlayfieldBuildStep::PlayerProxy:
            return "Local skater mesh at camera focus (same as playing draw).";
        case PlayfieldBuildStep::Puck:
            return "Puck cylinder at rink center.";
        case PlayfieldBuildStep::OpponentSkater:
            return "Team B skater near host spawn (fixed rink position).";
        case PlayfieldBuildStep::GameplayFx:
            return "Waypoint disc + shield ring. LMB on ice sets waypoint; X clears.";
        case PlayfieldBuildStep::FullPlayingPath:
            return "Solo: exact PlayingScene3D path. Cumulative: all prior layers.";
        case PlayfieldBuildStep::Count:
            break;
    }
    return "";
}

[[nodiscard]] PlayfieldBuildStep step_from_index(int const index) noexcept {
    int const max = playfield_build_step_count() - 1;
    int const clamped = std::clamp(index, 0, max);
    return static_cast<PlayfieldBuildStep>(clamped);
}

void draw_spawn_axes(float spawn_x, float spawn_y) noexcept {
    Vector3 const base = sim_to_world3(spawn_x, spawn_y, 0.f);
    float const len = 80.f;
    Vector3 const x_tip = sim_to_world3(spawn_x + len, spawn_y, 0.f);
    Vector3 const z_tip = sim_to_world3(spawn_x, spawn_y + len, 0.f);
    Vector3 const y_tip = Vector3{base.x, base.y + 60.f, base.z};

    DrawCube(x_tip, 6.f, 6.f, 6.f, Color{255, 60, 60, 255});
    DrawCube(y_tip, 6.f, 6.f, 6.f, Color{60, 255, 80, 255});
    DrawCube(z_tip, 6.f, 6.f, 6.f, Color{80, 120, 255, 255});
    DrawCube(base, 8.f, 8.f, 8.f, Color{255, 220, 80, 255});
}

[[nodiscard]] MapPoint demo_spawn() noexcept {
    return map_runtime().host_spawn_or_default();
}

}  // namespace

bool boot_render_test_scene(int argc, char **argv) noexcept {
    if (!kBootRenderTestScene) {
        return false;
    }

    if (argc > 0 && argv != nullptr) {
        for (int i = 1; i < argc; ++i) {
            if (argv[i] == nullptr) {
                continue;
            }
            if (std::strcmp(argv[i], "--home") == 0 ||
                std::strcmp(argv[i], "--no-render-test") == 0) {
                return false;
            }
            if (std::strcmp(argv[i], "--render-test") == 0) {
                return true;
            }
        }
    }

    if (char const *env = std::getenv("ZH_RENDER_TEST")) {
        if (env[0] == '0' || env[0] == 'f' || env[0] == 'F' || env[0] == 'n' || env[0] == 'N') {
            return false;
        }
        if (env[0] == '1' || env[0] == 't' || env[0] == 'T' || env[0] == 'y' || env[0] == 'Y') {
            return true;
        }
    }

    return true;
}

void RenderTestScene::reset_waypoint_demo() noexcept {
    MapPoint const spawn = map_runtime().host_spawn_or_default();
    waypoint_x_ = spawn.x + 90.f;
    waypoint_y_ = spawn.y + 50.f;
    waypoint_active_ = true;
}

void RenderTestScene::reset_camera() noexcept {
    fly_yaw_deg_ = 45.f;
    fly_pitch_deg_ = 25.f;
    fly_speed_ = 480.f;
    mouse_prev_valid_ = false;
    view_fraction_ = kPlayingDefaultViewFraction;
    use_free_camera_ = true;
    show_ice_bounds_ = false;
    build_step_ = 0;
    cumulative_mode_ = true;
    MapPoint const spawn = map_runtime().host_spawn_or_default();
    focus_x_ = spawn.x;
    focus_y_ = spawn.y;
    reset_waypoint_demo();
    update_playing_camera();
    sync_free_camera_from_current();
}

void RenderTestScene::update_playing_camera() noexcept {
    float const view_w = static_cast<float>(GetScreenWidth());
    float const view_h = static_cast<float>(GetScreenHeight());
    camera_ = make_render_test_playing_camera(focus_x_, focus_y_, view_fraction_, view_w, view_h,
                                                rink_ice_bounds());
}

void RenderTestScene::sync_free_camera_from_current() noexcept {
    fly_x_ = camera_.position.x;
    fly_y_ = camera_.position.y;
    fly_z_ = camera_.position.z;

    Vector3 const to_target = Vector3Subtract(camera_.target, camera_.position);
    float const len_sq =
        to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z;
    if (len_sq < 1e-6f) {
        return;
    }
    float const inv_len = 1.f / std::sqrt(len_sq);
    Vector3 const look{to_target.x * inv_len, to_target.y * inv_len, to_target.z * inv_len};
    fly_pitch_deg_ = std::asin(std::clamp(-look.y, -1.f, 1.f)) * RAD2DEG;
    fly_yaw_deg_ = std::atan2(-look.x, -look.z) * RAD2DEG;
}

void RenderTestScene::update_free_camera(float const frame_dt) noexcept {
    float const turn_speed = 90.f;
    float const pitch_speed = 60.f;
    float const wheel = GetMouseWheelMove();

    if (IsKeyDown(KEY_LEFT)) {
        fly_yaw_deg_ -= turn_speed * frame_dt;
    }
    if (IsKeyDown(KEY_RIGHT)) {
        fly_yaw_deg_ += turn_speed * frame_dt;
    }
    if (IsKeyDown(KEY_UP)) {
        fly_pitch_deg_ = std::min(89.f, fly_pitch_deg_ + pitch_speed * frame_dt);
    }
    if (IsKeyDown(KEY_DOWN)) {
        fly_pitch_deg_ = std::max(-89.f, fly_pitch_deg_ - pitch_speed * frame_dt);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 const mp = GetMousePosition();
        if (mouse_prev_valid_) {
            float const dx = mp.x - prev_mouse_.x;
            float const dy = mp.y - prev_mouse_.y;
            fly_yaw_deg_ += dx * 0.25f;
            fly_pitch_deg_ = std::clamp(fly_pitch_deg_ + dy * 0.2f, -89.f, 89.f);
        }
        prev_mouse_ = mp;
        mouse_prev_valid_ = true;
    } else {
        mouse_prev_valid_ = false;
    }

    float const yaw = fly_yaw_deg_ * kDegToRad;
    float const pitch = fly_pitch_deg_ * kDegToRad;
    float const cos_p = std::cos(pitch);
    float const sin_p = std::sin(pitch);
    float const cos_y = std::cos(yaw);
    float const sin_y = std::sin(yaw);

    Vector3 const forward{-cos_p * cos_y, -sin_p, -cos_p * sin_y};
    Vector3 const right{sin_y, 0.f, -cos_y};

    float move_x = 0.f;
    float move_y = 0.f;
    float move_z = 0.f;
    if (IsKeyDown(KEY_W)) {
        move_x += forward.x;
        move_y += forward.y;
        move_z += forward.z;
    }
    if (IsKeyDown(KEY_S)) {
        move_x -= forward.x;
        move_y -= forward.y;
        move_z -= forward.z;
    }
    if (IsKeyDown(KEY_D)) {
        move_x += right.x;
        move_z += right.z;
    }
    if (IsKeyDown(KEY_A)) {
        move_x -= right.x;
        move_z -= right.z;
    }
    if (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_E)) {
        move_y += 1.f;
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_Q)) {
        move_y -= 1.f;
    }

    float speed = fly_speed_;
    if (IsKeyDown(KEY_LEFT_CONTROL)) {
        speed *= 4.f;
    }
    if (wheel != 0.f) {
        float const dolly_scale = std::clamp(fly_y_ / 120.f, 0.12f, 1.f);
        float const dolly = wheel * speed * 0.35f * dolly_scale;
        fly_x_ += forward.x * dolly;
        fly_y_ += forward.y * dolly;
        fly_z_ += forward.z * dolly;
    }

    float const move_len_sq = move_x * move_x + move_y * move_y + move_z * move_z;
    if (move_len_sq > 1e-6f) {
        float const inv = speed * frame_dt / std::sqrt(move_len_sq);
        fly_x_ += move_x * inv;
        fly_y_ += move_y * inv;
        fly_z_ += move_z * inv;
    }

    fly_y_ = std::clamp(fly_y_, kFlyMinHeightY, kFlyMaxHeightY);

    float const target_dist = std::clamp(fly_y_ * 0.45f, 50.f, 220.f);
    camera_.position = Vector3{fly_x_, fly_y_, fly_z_};
    camera_.target = Vector3{fly_x_ + forward.x * target_dist, fly_y_ + forward.y * target_dist,
                             fly_z_ + forward.z * target_dist};
    camera_.up = Vector3{0.f, 1.f, 0.f};
    camera_.fovy = 45.f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void RenderTestScene::update_step_input() noexcept {
    int const last = playfield_build_step_count() - 1;

    if (IsKeyPressed(KEY_LEFT_BRACKET) || IsKeyPressed(KEY_COMMA)) {
        build_step_ = std::max(0, build_step_ - 1);
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET) || IsKeyPressed(KEY_PERIOD)) {
        build_step_ = std::min(last, build_step_ + 1);
    }

    for (int digit = 0; digit <= 9; ++digit) {
        if (IsKeyPressed(KEY_ZERO + digit)) {
            build_step_ = std::min(digit, last);
        }
    }
    if (IsKeyPressed(KEY_MINUS)) {
        build_step_ = std::min(10, last);
    }
    if (IsKeyPressed(KEY_EQUAL)) {
        build_step_ = std::min(11, last);
    }

    if (IsKeyPressed(KEY_TAB)) {
        cumulative_mode_ = !cumulative_mode_;
    }
    if (IsKeyPressed(KEY_C)) {
        use_free_camera_ = !use_free_camera_;
        if (use_free_camera_) {
            sync_free_camera_from_current();
        } else {
            focus_x_ = fly_x_;
            focus_y_ = fly_z_;
            update_playing_camera();
        }
    }
    if (IsKeyPressed(KEY_G)) {
        show_ice_bounds_ = !show_ice_bounds_;
    }
    if (IsKeyPressed(KEY_R)) {
        reset_camera();
    }
}

void RenderTestScene::update_waypoint_input() noexcept {
    PlayfieldBuildStep const current = step_from_index(build_step_);
    if (current != PlayfieldBuildStep::GameplayFx &&
        current != PlayfieldBuildStep::FullPlayingPath) {
        return;
    }

    if (IsKeyPressed(KEY_X)) {
        waypoint_active_ = false;
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }

    Vector2 sim{};
    if (!screen_to_sim_ground(GetMousePosition(), camera_, sim)) {
        return;
    }

    waypoint_x_ = sim.x;
    waypoint_y_ = sim.y;
    clamp_waypoint_to_rink(waypoint_x_, waypoint_y_);
    waypoint_active_ = true;
}

void RenderTestScene::update_focus_pan(float const frame_dt) noexcept {
    if (use_free_camera_) {
        return;
    }

    float dx = 0.f;
    float dy = 0.f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        dx -= 1.f;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        dx += 1.f;
    }
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        dy -= 1.f;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        dy += 1.f;
    }
    if (dx == 0.f && dy == 0.f) {
        return;
    }

    float const len = std::sqrt(dx * dx + dy * dy);
    dx /= len;
    dy /= len;
    float const speed = kFocusPanSpeed * frame_dt * view_fraction_;
    focus_x_ += dx * speed;
    focus_y_ += dy * speed;
}

void RenderTestScene::update(float const frame_dt) noexcept {
    update_step_input();
    update_focus_pan(frame_dt);

    float const wheel = GetMouseWheelMove();
    if (!use_free_camera_ && wheel != 0.f) {
        view_fraction_ = std::max(kMinViewFraction,
                                  view_fraction_ * std::pow(kPlayingViewFractionWheelStep, -wheel));
    }

    if (use_free_camera_) {
        update_free_camera(frame_dt);
    } else {
        update_playing_camera();
    }

    update_waypoint_input();
}

void RenderTestScene::draw_origin_debug() const noexcept {
    MapPoint const spawn = demo_spawn();
    draw_spawn_axes(spawn.x, spawn.y);
}

PlayingEntityDraw RenderTestScene::host_entity_demo() const noexcept {
    MapPoint const spawn = demo_spawn();
    PlayingEntityDraw host{};
    host.active = true;
    host.x = spawn.x;
    host.y = spawn.y;
    host.is_local = true;
    return host;
}

void RenderTestScene::draw_ice_bounds_debug() const noexcept {
    RectF const ice = rink_ice_bounds();
    float const y = 2.f;
    Color const col{255, 40, 40, 220};
    float const marker = 10.f;
    auto corner = [&](float sx, float sy) {
        DrawCube(sim_to_world3(sx, sy, y), marker, 3.f, marker, col);
    };
    corner(ice.x, ice.y);
    corner(ice.x + ice.w, ice.y);
    corner(ice.x + ice.w, ice.y + ice.h);
    corner(ice.x, ice.y + ice.h);
}

void RenderTestScene::draw_puck_demo() const noexcept {
    draw_puck_3d(rink_center_x(), rink_mid_y());
}

void RenderTestScene::draw_opponent_demo() const noexcept {
    MapPoint const spawn = demo_spawn();
    PlayingEntityDraw opponent{};
    opponent.active = true;
    opponent.x = spawn.x + 140.f;
    opponent.y = spawn.y - 30.f;
    opponent.team_b = true;
    draw_skater_3d_entity(opponent);
}

void RenderTestScene::draw_gameplay_fx_demo() const noexcept {
    PlayingEntityDraw const host = host_entity_demo();
    if (waypoint_active_) {
        draw_waypoint_marker_3d(waypoint_x_, waypoint_y_);
    }
    draw_shield_ring_3d(host.x, host.y, 0.65f);
}

void RenderTestScene::apply_demo_waypoint(PlayingWorldDraw &world) const noexcept {
    world.host.has_waypoint = waypoint_active_;
    world.host.way_x = waypoint_x_;
    world.host.way_y = waypoint_y_;
}

void RenderTestScene::draw_world() const noexcept {
    MapDefinition const &map = map_runtime().definition();
    PlayfieldBuildStep const current = step_from_index(build_step_);

    if (!cumulative_mode_ && current == PlayfieldBuildStep::FullPlayingPath) {
        MapPoint const spawn = demo_spawn();
        PlayingWorldDraw world = make_render_test_demo_world(spawn.x, spawn.y);
        apply_demo_waypoint(world);
        playing_scene_.draw_frame(camera_, world);
        return;
    }

    BeginMode3D(camera_);

    auto draw_step_3d = [&](PlayfieldBuildStep const step) {
        switch (step) {
            case PlayfieldBuildStep::Origin:
                draw_origin_debug();
                return;
            case PlayfieldBuildStep::PlayerProxy:
                draw_skater_3d_entity(host_entity_demo());
                return;
            case PlayfieldBuildStep::Puck:
                draw_puck_demo();
                return;
            case PlayfieldBuildStep::OpponentSkater:
                draw_opponent_demo();
                return;
            case PlayfieldBuildStep::GameplayFx:
                draw_gameplay_fx_demo();
                return;
            case PlayfieldBuildStep::FullPlayingPath:
                return;
            default:
                draw_playfield_build_step_3d(map, step);
                return;
        }
    };

    if (cumulative_mode_) {
        for (int i = 0; i <= build_step_; ++i) {
            PlayfieldBuildStep const step = step_from_index(i);
            if (step == PlayfieldBuildStep::FullPlayingPath) {
                continue;
            }
            draw_step_3d(step);
        }
    } else {
        if (current != PlayfieldBuildStep::FullPlayingPath) {
            draw_step_3d(current);
        }
    }

    if (show_ice_bounds_) {
        draw_ice_bounds_debug();
    }

    EndMode3D();
}

void RenderTestScene::draw_hud() const noexcept {
    PlayfieldBuildStep const current = step_from_index(build_step_);
    int const fs = 20;
    int y = 14;

    auto line = [&](char const *text) {
        DrawText(text, 14, y, fs, BLACK);
        y += fs + 5;
    };

    line("Playfield build test (one piece at a time)");
    line("[ / ] or , / . : prev/next    0-9 jump step    - / = : steps 10-11");
    line("Tab: solo/cumulative   G: ice bounds   R: reset   Esc: home");
    line("Top-down: wheel=zoom out (unbounded)   WASD/keys=pan focus");
    line("Free fly (C): WASD move   Space/E up   Shift/Q down   Ctrl=fast");
    line("Free fly: RMB/arrows look   wheel=dolly in/out (unbounded)");

    char buf[192];
    std::snprintf(buf, sizeof(buf), "Step %d/%d  %s  mode: %s  camera: %s", build_step_,
                  playfield_build_step_count() - 1, playfield_build_step_name(current),
                  cumulative_mode_ ? "cumulative" : "solo",
                  use_free_camera_ ? "free fly" : "playing top-down");
    line(buf);

    if (!cumulative_mode_) {
        line("Solo mode: only the current step is drawn.");
    }
    line(step_hint(current));

    if (current == PlayfieldBuildStep::GameplayFx ||
        current == PlayfieldBuildStep::FullPlayingPath) {
        if (waypoint_active_) {
            std::snprintf(buf, sizeof(buf), "Waypoint (%.0f, %.0f)  LMB=move  X=clear", waypoint_x_,
                          waypoint_y_);
        } else {
            std::snprintf(buf, sizeof(buf), "Waypoint: none  LMB on ice to place  X=clear");
        }
        line(buf);
    }

    if (use_free_camera_) {
        std::snprintf(buf, sizeof(buf),
                      "fly (%.0f, %.0f, %.0f)  yaw %.0f pitch %.0f  bounds: %s", fly_x_, fly_y_,
                      fly_z_, fly_yaw_deg_, fly_pitch_deg_, show_ice_bounds_ ? "on" : "off");
    } else {
        std::snprintf(buf, sizeof(buf),
                      "focus (%.0f, %.0f)  zoom frac %.3f  cam (%.0f, %.0f, %.0f)  bounds: %s",
                      focus_x_, focus_y_, view_fraction_, camera_.position.x, camera_.position.y,
                      camera_.position.z, show_ice_bounds_ ? "on" : "off");
    }
    line(buf);
}

}  // namespace zh::ui
