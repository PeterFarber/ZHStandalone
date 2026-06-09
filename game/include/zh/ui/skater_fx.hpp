#pragma once

#include <array>
#include <cstdint>

namespace zh::ui {

enum class SkaterFxSlot : std::uint8_t {
    Host = 0,
    Remote0 = 1,
    Remote1 = 2,
    Remote2 = 3,
    Remote3 = 4,
    Count = 5,
};

class SkaterFxSystem {
  public:
    void reset() noexcept;

    // Call once per playing frame before feed().
    void begin_frame() noexcept;

    // boost_trail: Z overspeed window. slide_spray: X slide decel (or inferred on remote peers).
    void feed(SkaterFxSlot slot,
              float px,
              float py,
              float vx,
              float vy,
              bool boost_trail,
              bool slide_spray) noexcept;

    void update(float dt) noexcept;
    void draw_3d() const noexcept;

  private:
    struct SlotState {
        float px{0.f};
        float py{0.f};
        float vx{0.f};
        float vy{0.f};
        float prev_speed{0.f};
        bool fed{false};
        bool boost_trail{false};
        bool slide_spray{false};
        bool slide_was_active{false};
        float inferred_slide_timer{0.f};
        float ice_drip_accum{0.f};
        float boost_trail_accum{0.f};
    };

    struct TrailGhost {
        float x{0.f};
        float y{0.f};
        float age{0.f};
        float max_age{0.f};
        float radius{0.f};
    };

    struct IceParticle {
        float x{0.f};
        float y{0.f};
        float vx{0.f};
        float vy{0.f};
        float age{0.f};
        float max_age{0.f};
        float radius{0.f};
        unsigned char alpha{255};
    };

    static constexpr std::size_t kTrailCap = 32U;
    static constexpr std::size_t kParticleCap = 120U;

    void spawn_boost_trail_(SkaterFxSlot slot, float px, float py, float nx, float ny, float speed) noexcept;
    void spawn_slide_burst_(float px, float py, float nx, float ny, float speed) noexcept;
    void spawn_ice_drip_(float px, float py, float nx, float ny, float speed) noexcept;
    void push_particle_(IceParticle p) noexcept;

    std::array<SlotState, static_cast<std::size_t>(SkaterFxSlot::Count)> slots_{};
    std::array<std::array<TrailGhost, kTrailCap>, static_cast<std::size_t>(SkaterFxSlot::Count)> trails_{};
    std::array<std::size_t, static_cast<std::size_t>(SkaterFxSlot::Count)> trail_wr_{};
    std::array<IceParticle, kParticleCap> particles_{};
    std::size_t particle_count_{0};
};

}  // namespace zh::ui
