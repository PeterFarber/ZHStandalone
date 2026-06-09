// One authoritative host tick while match_running_: apply inputs, sim, broadcast snapshot.
#include "detail/app_context.hpp"

#include "zh/app.hpp"
#include "zh/gfx/gfx_compat.hpp"
#include "zh/net/host/host_snapshot_broadcast.hpp"

namespace zh::detail {

void AppContext::sim_match(float /*dt*/) {
    using namespace zh::game;

    // Order matters for determinism — bump protocol version if you reorder this.
    // delayed inputs → cooldowns → tick_host_match → ability edges → server_tick++ → snapshot

    if (!match_running_ || remote_client_) {
        return;
    }

    if (listen_server_ == nullptr || !listen_server_->valid()) {
        return;
    }

    host_render_prev_sk_x_ = world_.host_sk_x;
    host_render_prev_sk_y_ = world_.host_sk_y;
    host_render_prev_puck_x_ = world_.puck_x;
    host_render_prev_puck_y_ = world_.puck_y;

    zh::net::host::HostPeerSlotTablesMut slots{
        slot_active_,       slot_peer_,         slot_last_seq_,     slot_move_,
        slot_ability_,      slot_mouse_x_,      slot_mouse_y_,      slot_mouse_buttons_,
        remote_input_schedule_, remote_mouse_seen_, occupied_slots_mask_};

    world_.tick_match_clock(slot_active_, static_cast<float>(GetScreenWidth()),
                            static_cast<float>(GetScreenHeight()));

    if (world_.phase == zh::game::MatchPhase::Playing) {
        std::uint32_t const execute_tick = world_.server_tick + 1U;
        host_router_.apply_scheduled_inputs(execute_tick, world_.input_delay_ticks_b, slots);

        world_.decrement_match_cooldowns();

        // Re-read keys/mouse every sim step (held keys + RMB edge in tick_host_match).
        LocalPlayingInputSample const local = build_local_playing_input_fresh();
        HostMatchTickInputs tick_in{};
        tick_in.local.move_axes = local.move_axes;
        tick_in.local.mouse_buttons = local.mouse_buttons;
        tick_in.local.ability_axes = local.ability_axes;
        tick_in.local.mouse_x = local.mouse_x;
        tick_in.local.mouse_y = local.mouse_y;
        tick_in.slot_mouse_buttons = slot_mouse_buttons_;
        tick_in.slot_ability = slot_ability_;
        tick_in.slot_mouse_x = slot_mouse_x_;
        tick_in.slot_mouse_y = slot_mouse_y_;

        HostMatchTickResult const tick_result = world_.tick_host_match(
            tick_in, App::kFixedDt, slot_active_, remote_mouse_seen_,
            static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
        play_match_sounds(tick_result.sounds);

        world_.tick_end_boost_and_abilities(slot_active_, tick_in.local.ability_axes,
                                            slot_ability_);
    } else if (world_.phase == zh::game::MatchPhase::FaceoffCountdown) {
        // Face-off: only mouse clicks matter for early movement hints.
        LocalPlayingInputSample const local = build_local_playing_input_fresh();
        HostMatchTickInputs tick_in{};
        tick_in.local.mouse_buttons = local.mouse_buttons;
        tick_in.local.mouse_x = local.mouse_x;
        tick_in.local.mouse_y = local.mouse_y;
        tick_in.slot_mouse_buttons = slot_mouse_buttons_;
        (void)world_.tick_host_match(tick_in, App::kFixedDt, slot_active_, remote_mouse_seen_,
                                     static_cast<float>(GetScreenWidth()),
                                     static_cast<float>(GetScreenHeight()));
    }

    ++world_.server_tick;

    zh::net::host::HostSnapshotBroadcastContext snap_ctx{
        world_,           slot_last_seq_,     slot_active_,       occupied_slots_mask_,
        lobby_host_team_, lobby_slot_team_b_mask_, lobby_host_goalie_, lobby_slot_goalie_mask_};
    zh::net::host::broadcast_match_snapshot_if_due(*listen_server_, snap_ctx, world_.server_tick);
}

}  // namespace zh::detail
