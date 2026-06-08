#pragma once

// Host and remote slot velocity from an interpolated snapshot pair.

#include "zh/client/client_interp_frame.hpp"

#include <cstddef>

namespace zh::client {

// Host skater planar velocity estimated from interpolated snapshot pair.
void interp_host_vel(ClientInterpFrame const &frame,
                     float &vx_out,
                     float &vy_out,
                     float simulation_fixed_dt_sec) noexcept;

// Remote slot planar velocity from frame (slot_i < kRemoteSlots).
void interp_slot_vel(ClientInterpFrame const &frame,
                     std::size_t slot_i,
                     float &vx_out,
                     float &vy_out,
                     float simulation_fixed_dt_sec) noexcept;

}  // namespace zh::client
