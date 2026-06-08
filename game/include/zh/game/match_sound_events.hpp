#pragma once

// Gameplay sound triggers emitted by host sim (shared) and local-only ability cues.

namespace zh::game {

struct MatchSoundEvents {
    bool boost{false};
    bool one_timer{false};  // played locally only — not emitted from host sim tick
    bool shield{false};     // goalie shield — played locally only
    bool shot{false};
    bool puck_collision{false};
    bool puck_metal_collision{false};
    bool faceoff_countdown{false};
};

}  // namespace zh::game
