#include "zh/gfx/vulkan/vk_font.hpp"

#include "detail/resource_paths.hpp"
#include "zh/gfx/vulkan/font_bytes.hpp"
#include "zh/gfx/vulkan/vk_draw2d.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>

namespace zh::gfx::vk {

struct VkFont::State {
    stbtt_fontinfo font{};
    std::vector<unsigned char> ttf_bytes{};
    float base_scale{1.f};
    int ascent{0};
    bool ready{false};
};

namespace {

inline constexpr char const *kFontCandidates[] = {
    "fonts/game.woff2",
    "fonts/game.ttf",
    "fonts/ui.woff2",
    "fonts/ui.ttf",
    "fonts/default.woff2",
    "fonts/default.ttf",
};

[[nodiscard]] bool font_extension_supported(std::string const &ext) {
    return ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".woff2";
}

[[nodiscard]] char const *resolve_font_path() {
    static char path[512];

    for (char const *rel : kFontCandidates) {
        if (char const *hit = zh::detail::resolve_resource_file(rel)) {
            return hit;
        }
    }

    std::filesystem::path const dirs[] = {
        std::filesystem::path("resources") / "fonts",
        std::filesystem::path("..") / "resources" / "fonts",
    };
    for (auto const &dir : dirs) {
        if (!std::filesystem::is_directory(dir)) {
            continue;
        }
        for (auto const &entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            auto const ext = entry.path().extension().string();
            std::string lower = ext;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!font_extension_supported(lower)) {
                continue;
            }
            auto const s = entry.path().string();
            std::snprintf(path, sizeof(path), "%s", s.c_str());
            return path;
        }
    }

#if defined(_WIN32)
    char const *system[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (char const *c : system) {
        if (std::filesystem::exists(c)) {
            std::snprintf(path, sizeof(path), "%s", c);
            return path;
        }
    }
#endif
    return nullptr;
}

// Decode one UTF-8 codepoint; advances *cursor on success.
[[nodiscard]] int utf8_next(char const **cursor) {
    auto const *p = reinterpret_cast<unsigned char const *>(*cursor);
    if (*p == 0) {
        return 0;
    }
    if (*p < 0x80) {
        *cursor += 1;
        return static_cast<int>(*p);
    }
    if ((*p & 0xE0) == 0xC0 && p[1] != 0) {
        int const cp = static_cast<int>((*p & 0x1F) << 6 | (p[1] & 0x3F));
        *cursor += 2;
        return cp;
    }
    if ((*p & 0xF0) == 0xE0 && p[1] != 0 && p[2] != 0) {
        int const cp =
            static_cast<int>((*p & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F));
        *cursor += 3;
        return cp;
    }
    if ((*p & 0xF8) == 0xF0 && p[1] != 0 && p[2] != 0 && p[3] != 0) {
        int const cp = static_cast<int>((*p & 0x07) << 18 | (p[1] & 0x3F) << 12 |
                                        (p[2] & 0x3F) << 6 | (p[3] & 0x3F));
        *cursor += 4;
        return cp;
    }
    *cursor += 1;
    return static_cast<int>('?');
}

void draw_glyph_spans(Draw2dBatch &batch, unsigned char const *bitmap, int w, int h, float x0,
                      float y0, Color color) {
    if (bitmap == nullptr || w <= 0 || h <= 0) {
        return;
    }
    for (int y = 0; y < h; ++y) {
        int x = 0;
        while (x < w) {
            while (x < w && bitmap[y * w + x] <= 48) {
                ++x;
            }
            if (x >= w) {
                break;
            }
            int const span_start = x;
            int peak_alpha = static_cast<int>(bitmap[y * w + x]);
            while (x < w && bitmap[y * w + x] > 48) {
                peak_alpha = std::max(peak_alpha, static_cast<int>(bitmap[y * w + x]));
                ++x;
            }
            Color span_col = color;
            span_col.a = static_cast<unsigned char>(
                std::min(255, static_cast<int>(color.a) * peak_alpha / 255));
            float const span_w = static_cast<float>(x - span_start);
            if (span_w > 0.f) {
                batch.fill_rect(x0 + static_cast<float>(span_start), y0 + static_cast<float>(y),
                                span_w, 1.f, span_col);
            }
        }
    }
}

}  // namespace

bool VkFont::init(VkContext const &) {
    if (state_ != nullptr) {
        return true;
    }
    char const *path = resolve_font_path();
    if (path == nullptr) {
        return false;
    }

    auto st = std::make_unique<State>();
    if (!load_font_bytes(path, st->ttf_bytes)) {
        std::fprintf(stderr, "[zh/gfx] UI font load failed: %s\n", path);
        return false;
    }
    if (stbtt_InitFont(&st->font, st->ttf_bytes.data(),
                       stbtt_GetFontOffsetForIndex(st->ttf_bytes.data(), 0)) == 0) {
        std::fprintf(stderr, "[zh/gfx] UI font parse failed: %s\n", path);
        return false;
    }
    st->base_scale = stbtt_ScaleForPixelHeight(&st->font, 32.f);
    int ascent_px = 0;
    stbtt_GetFontVMetrics(&st->font, &ascent_px, nullptr, nullptr);
    st->ascent = static_cast<int>(static_cast<float>(ascent_px) * st->base_scale);
    st->ready = true;

    std::fprintf(stderr, "[zh/gfx] UI font: %s\n", path);
    state_ = st.release();
    return true;
}

void VkFont::shutdown(VkContext const &) {
    delete state_;
    state_ = nullptr;
}

int VkFont::measure_text(char const *text, int font_size) const {
    if (state_ == nullptr || !state_->ready || text == nullptr || font_size <= 0) {
        return 0;
    }
    float const scale = static_cast<float>(font_size) / 32.f;
    float width = 0.f;
    for (char const *p = text; *p != '\0';) {
        int const cp = utf8_next(&p);
        if (cp == 0) {
            break;
        }
        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&state_->font, cp, &advance, &lsb);
        width += static_cast<float>(advance) * state_->base_scale * scale;
    }
    return static_cast<int>(width);
}

void VkFont::draw_text(Draw2dBatch &batch, char const *text, int pos_x, int pos_y, int font_size,
                       Color color) const {
    if (state_ == nullptr || !state_->ready || text == nullptr || font_size <= 0) {
        return;
    }
    float const scale = static_cast<float>(font_size) / 32.f;
    float cursor_x = static_cast<float>(pos_x);
    float const baseline_y =
        static_cast<float>(pos_y) + static_cast<float>(state_->ascent) * scale;

    for (char const *p = text; *p != '\0';) {
        int const cp = utf8_next(&p);
        if (cp == 0) {
            break;
        }

        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&state_->font, cp, &advance, &lsb);
        cursor_x += static_cast<float>(lsb) * state_->base_scale * scale;

        int w = 0;
        int h = 0;
        int xoff = 0;
        int yoff = 0;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(
            &state_->font, 0, state_->base_scale * scale, cp, &w, &h, &xoff, &yoff);
        if (bitmap != nullptr && w > 0 && h > 0) {
            draw_glyph_spans(batch, bitmap, w, h,
                             cursor_x + static_cast<float>(xoff),
                             baseline_y + static_cast<float>(yoff), color);
            stbtt_FreeBitmap(bitmap, nullptr);
        }
        cursor_x += static_cast<float>(advance) * state_->base_scale * scale;
    }
}

}  // namespace zh::gfx::vk
