// Planar velocity from interpolated snapshot pair (fixed_dt tick scale).

#include "zh/client/interp_velocity.hpp"

namespace zh::client {

void interp_host_vel(ClientInterpFrame const &frame,
                     float &vx_out,
                     float &vy_out,
                     float simulation_fixed_dt_sec) noexcept {
    // Same server_tick on both ends → no displacement to divide.
    if (frame.newer_snap.server_tick <= frame.older_snap.server_tick) {
        vx_out = 0.f;
        vy_out = 0.f;
        return;
    }
    float const sdt = static_cast<float>(frame.newer_snap.server_tick - frame.older_snap.server_tick) *
                      simulation_fixed_dt_sec;
    vx_out = (frame.newer_snap.host_px - frame.older_snap.host_px) / sdt;
    vy_out = (frame.newer_snap.host_py - frame.older_snap.host_py) / sdt;
}

void interp_slot_vel(ClientInterpFrame const &frame,
                     std::size_t const slot_i,
                     float &vx_out,
                     float &vy_out,
                     float const simulation_fixed_dt_sec) noexcept {
    if (frame.newer_snap.server_tick <= frame.older_snap.server_tick) {
        vx_out = 0.f;
        vy_out = 0.f;
        return;
    }
    float const sdt = static_cast<float>(frame.newer_snap.server_tick - frame.older_snap.server_tick) *
                      simulation_fixed_dt_sec;
    vx_out = (frame.newer_snap.skater_px[slot_i] - frame.older_snap.skater_px[slot_i]) / sdt;
    vy_out = (frame.newer_snap.skater_py[slot_i] - frame.older_snap.skater_py[slot_i]) / sdt;
}

}  // namespace zh::client
