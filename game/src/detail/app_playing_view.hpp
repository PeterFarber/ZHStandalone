#pragma once

// Playing match view assembly: FX feed, world draw model, HUD model (detail layer).

#include "zh/client/client_interp_frame.hpp"
#include "zh/ui/playing_hud.hpp"
#include "zh/ui/playing_scene_3d.hpp"

namespace zh::detail {

struct AppContext;

struct PlayingViewFrame {
    zh::client::ClientInterpFrame interp{};
    float mix_t{1.f};
    bool host_world{false};
    float host_render_alpha{0.f};
};

[[nodiscard]] PlayingViewFrame make_playing_view_frame(AppContext const &ctx) noexcept;

void feed_playing_skater_fx(AppContext &ctx, PlayingViewFrame const &view,
                            float frame_dt) noexcept;

[[nodiscard]] zh::ui::PlayingWorldDraw build_playing_world_draw(AppContext const &ctx,
                                                                  PlayingViewFrame const &view);

void fill_playing_hud_model(AppContext const &ctx, PlayingViewFrame const &view,
                            zh::ui::PlayingHudModel &out) noexcept;

void draw_faceoff_countdown_overlay(int countdown_secs) noexcept;

}  // namespace zh::detail
