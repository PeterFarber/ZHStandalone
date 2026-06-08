// Boost speed trail and slide-stop ice chip particles (drawn in world space).

#include "zh/ui/skater_fx.hpp"

#include "zh/game/constants.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace zh::ui {

namespace {

float norm_or_default(float x, float y, float &nx, float &ny) noexcept {
    float const len = std::hypot(x, y);
    if (len < 1e-3f) {
        nx = 0.f;
        ny = -1.f;
        return 0.f;
    }
    nx = x / len;
    ny = y / len;
    return len;
}

Color trail_color(float t) noexcept {
    // t: 0 = fresh, 1 = faded
    unsigned char const a = static_cast<unsigned char>(std::clamp((1.f - t) * 120.f, 0.f, 255.f));
    return Color{120, 210, 255, a};
}

Color ice_color(float t) noexcept {
    unsigned char const a = static_cast<unsigned char>(std::clamp((1.f - t) * 220.f, 0.f, 255.f));
    return Color{static_cast<unsigned char>(228 - static_cast<int>(t * 40.f)),
                 static_cast<unsigned char>(240 - static_cast<int>(t * 20.f)), 255, a};
}

}  // namespace

void SkaterFxSystem::reset() noexcept {
    slots_ = {};
    trails_ = {};
    trail_wr_ = {};
    particles_ = {};
    particle_count_ = 0;
}

void SkaterFxSystem::begin_frame() noexcept {
    for (SlotState &s : slots_) {
        s.fed = false;
        s.boost_trail = false;
        s.slide_spray = false;
    }
}

void SkaterFxSystem::feed(SkaterFxSlot const slot,
                          float const px,
                          float const py,
                          float const vx,
                          float const vy,
                          bool const boost_trail,
                          bool const slide_spray) noexcept {
    auto const idx = static_cast<std::size_t>(slot);
    if (idx >= slots_.size()) {
        return;
    }
    SlotState &s = slots_[idx];
    s.px = px;
    s.py = py;
    s.vx = vx;
    s.vy = vy;
    s.fed = true;
    s.boost_trail = boost_trail;
    s.slide_spray = slide_spray;
}

void SkaterFxSystem::spawn_boost_trail_(SkaterFxSlot const slot,
                                        float const px,
                                        float const py,
                                        float const nx,
                                        float const ny,
                                        float const speed) noexcept {
    auto const idx = static_cast<std::size_t>(slot);
    std::size_t const wr = trail_wr_[idx] % kTrailCap;
    trail_wr_[idx] = wr + 1U;

    float const back = 10.f + std::min(speed * 0.035f, 18.f);
    trails_[idx][wr] = TrailGhost{
        px - nx * back,
        py - ny * back,
        0.f,
        0.18f + std::min(speed / 1200.f, 0.08f),
        3.5f + std::min(speed * 0.012f, 5.f),
    };
}

void SkaterFxSystem::push_particle_(IceParticle p) noexcept {
    if (particle_count_ >= kParticleCap) {
        particle_count_ = kParticleCap / 2U;
    }
    particles_[particle_count_++] = p;
}

void SkaterFxSystem::spawn_slide_burst_(float const px,
                                        float const py,
                                        float const nx,
                                        float const ny,
                                        float const speed) noexcept {
    float const pxn = -nx;
    float const pyn = -ny;
    float const spread = 0.55f + std::min(speed / 400.f, 0.35f);
    for (int i = 0; i < 16; ++i) {
        float const u = static_cast<float>(i) / 15.f;
        float const ang = (u - 0.5f) * spread + (GetRandomValue(-12, 12) / 100.f);
        float bx = pxn;
        float by = pyn;
        float const ca = std::cos(ang);
        float const sa = std::sin(ang);
        float const dir_x = bx * ca - by * sa;
        float const dir_y = bx * sa + by * ca;
        float const spd = 80.f + GetRandomValue(0, 140) + speed * 0.15f;
        push_particle_(IceParticle{
            px + dir_x * 4.f,
            py + dir_y * 4.f,
            dir_x * spd,
            dir_y * spd,
            0.f,
            0.35f + GetRandomValue(0, 20) / 100.f,
            1.5f + GetRandomValue(0, 20) / 10.f,
            255,
        });
    }
}

void SkaterFxSystem::spawn_ice_drip_(float const px,
                                     float const py,
                                     float const nx,
                                     float const ny,
                                     float const speed) noexcept {
    float const pxn = -nx;
    float const pyn = -ny;
    float const jitter = GetRandomValue(-10, 10) / 100.f;
    float const dir_x = pxn + ny * jitter;
    float const dir_y = pyn - nx * jitter;
    float const len = std::hypot(dir_x, dir_y);
    float const dx = len > 1e-3f ? dir_x / len : pxn;
    float const dy = len > 1e-3f ? dir_y / len : pyn;
    push_particle_(IceParticle{
        px + dx * 6.f,
        py + dy * 6.f,
        dx * (40.f + speed * 0.08f),
        dy * (40.f + speed * 0.08f),
        0.f,
        0.22f,
        1.2f + GetRandomValue(0, 8) / 10.f,
        210,
    });
}

void SkaterFxSystem::update(float const dt) noexcept {
    float const clamped_dt = std::clamp(dt, 0.f, 0.05f);

    for (std::size_t si = 0; si < slots_.size(); ++si) {
        SlotState &s = slots_[si];
        if (!s.fed) {
            s.slide_was_active = false;
            s.inferred_slide_timer = std::max(0.f, s.inferred_slide_timer - clamped_dt);
            continue;
        }

        float nx = 0.f;
        float ny = 0.f;
        float const speed = norm_or_default(s.vx, s.vy, nx, ny);

        bool slide_active = s.slide_spray;
        if (!slide_active && s.prev_speed > 95.f && speed > 28.f && speed < s.prev_speed * 0.82f) {
            s.inferred_slide_timer = 0.34f;
        }
        if (s.inferred_slide_timer > 0.f) {
            slide_active = true;
            s.inferred_slide_timer = std::max(0.f, s.inferred_slide_timer - clamped_dt);
        }

        // Trail only while boost overspeed is active (Z zoom), not normal top speed.
        if (s.boost_trail && speed > 40.f) {
            s.boost_trail_accum += clamped_dt;
            while (s.boost_trail_accum >= 0.045f) {
                spawn_boost_trail_(static_cast<SkaterFxSlot>(si), s.px, s.py, nx, ny, speed);
                s.boost_trail_accum -= 0.045f;
            }
        } else {
            s.boost_trail_accum = 0.f;
        }

        if (slide_active && !s.slide_was_active && speed > 35.f) {
            spawn_slide_burst_(s.px, s.py, nx, ny, speed);
        }

        if (slide_active && speed > 22.f) {
            s.ice_drip_accum += clamped_dt;
            while (s.ice_drip_accum >= 0.04f) {
                spawn_ice_drip_(s.px, s.py, nx, ny, speed);
                s.ice_drip_accum -= 0.04f;
            }
        } else {
            s.ice_drip_accum = 0.f;
        }

        s.slide_was_active = slide_active;
        s.prev_speed = speed;
    }

    for (std::size_t si = 0; si < trails_.size(); ++si) {
        float const fade_mul =
            (slots_[si].fed && !slots_[si].boost_trail) ? 3.5f : 1.f;
        for (TrailGhost &g : trails_[si]) {
            if (g.max_age <= 0.f) {
                continue;
            }
            g.age += clamped_dt * fade_mul;
            if (g.age > g.max_age) {
                g.max_age = 0.f;
            }
        }
    }

    std::size_t write = 0;
    for (std::size_t i = 0; i < particle_count_; ++i) {
        IceParticle p = particles_[i];
        p.age += clamped_dt;
        p.x += p.vx * clamped_dt;
        p.y += p.vy * clamped_dt;
        p.vx *= 0.92f;
        p.vy *= 0.92f;
        if (p.age < p.max_age) {
            particles_[write++] = p;
        }
    }
    particle_count_ = write;
}

void SkaterFxSystem::draw() noexcept {
    for (std::size_t si = 0; si < trails_.size(); ++si) {
        for (TrailGhost const &g : trails_[si]) {
            if (g.max_age <= 0.f || g.age >= g.max_age) {
                continue;
            }
            float const t = g.age / g.max_age;
            Color const col = trail_color(t);
            DrawCircleV({g.x, g.y}, g.radius * (1.f - t * 0.35f), col);
            if (col.a > 40) {
                DrawCircleV({g.x, g.y}, g.radius * 0.35f * (1.f - t),
                            Color{210, 245, 255, static_cast<unsigned char>(col.a / 2)});
            }
        }
    }

    for (std::size_t i = 0; i < particle_count_; ++i) {
        IceParticle const &p = particles_[i];
        if (p.max_age <= 0.f) {
            continue;
        }
        float const t = p.age / p.max_age;
        Color col = ice_color(t);
        col.a = static_cast<unsigned char>(std::min(col.a, p.alpha));
        DrawCircleV({p.x, p.y}, p.radius * (1.f - t * 0.4f), col);
    }
}

}  // namespace zh::ui
