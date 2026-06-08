#pragma once

// Two snapshots framing render time plus mix_t (0 = older, 1 = newer).

#include "zh/protocol.hpp"

namespace zh::client {

struct ClientInterpFrame {
    PackedSnapshot older_snap{};
    PackedSnapshot newer_snap{};
    float mix_t{1.f};
};

}  // namespace zh::client
