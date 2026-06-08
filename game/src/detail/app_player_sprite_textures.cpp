// Load skater/goalie PNGs; team B is a hue shift of team A art.

#include "detail/app_context.hpp"
#include "detail/resource_paths.hpp"

#include <raylib.h>

namespace zh {

namespace {

constexpr float kPlayerSpriteHueShiftEastDeg = 172.f;

[[nodiscard]] char const *resolve_sprite_path(char const *rel) noexcept {
    return zh::detail::resolve_resource_file(rel);
}

void hue_shift_image_hsv(Image *im, float delta_h_deg) noexcept {
    if (im == nullptr || im->data == nullptr || im->width <= 0 || im->height <= 0) {
        return;
    }
    ImageFormat(im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    for (int y = 0; y < im->height; ++y) {
        for (int x = 0; x < im->width; ++x) {
            Color const c = GetImageColor(*im, x, y);
            if (c.a == 0) {
                continue;
            }
            Vector3 hsv = ColorToHSV(c);
            hsv.x += delta_h_deg;
            while (hsv.x < 0.f) {
                hsv.x += 360.f;
            }
            while (hsv.x >= 360.f) {
                hsv.x -= 360.f;
            }
            Color o = ColorFromHSV(hsv.x, hsv.y, hsv.z);
            o.a = c.a;
            ImageDrawPixel(im, x, y, o);
        }
    }
}

[[nodiscard]] bool load_team_pair(char const *rel,
                                  char const *fallback_rel,
                                  Texture2D &out_a,
                                  Texture2D &out_b) noexcept {
    char const *path = resolve_sprite_path(rel);
    if (path == nullptr && fallback_rel != nullptr) {
        path = resolve_sprite_path(fallback_rel);
    }
    if (path == nullptr) {
        return false;
    }
    Image base = LoadImage(path);
    if (base.data == nullptr || base.width <= 0 || base.height <= 0) {
        UnloadImage(base);
        return false;
    }
    Image img_a = ImageCopy(base);
    Image img_b = ImageCopy(base);
    UnloadImage(base);
    if (img_a.data == nullptr || img_b.data == nullptr) {
        UnloadImage(img_a);
        UnloadImage(img_b);
        return false;
    }
    hue_shift_image_hsv(&img_b, kPlayerSpriteHueShiftEastDeg);
    out_a = LoadTextureFromImage(img_a);
    out_b = LoadTextureFromImage(img_b);
    UnloadImage(img_a);
    UnloadImage(img_b);
    if (out_a.id != 0U) {
        SetTextureFilter(out_a, TEXTURE_FILTER_BILINEAR);
    }
    if (out_b.id != 0U) {
        SetTextureFilter(out_b, TEXTURE_FILTER_BILINEAR);
    }
    return out_a.id != 0U && out_b.id != 0U;
}

}  // namespace

void detail::AppContext::unload_player_team_textures_if_any() {
    if (!player_tex_ready_) {
        return;
    }
    UnloadTexture(player_tex_team_a_);
    UnloadTexture(player_tex_team_b_);
    UnloadTexture(player_goalie_tex_team_a_);
    UnloadTexture(player_goalie_tex_team_b_);
    player_tex_team_a_ = Texture2D{};
    player_tex_team_b_ = Texture2D{};
    player_goalie_tex_team_a_ = Texture2D{};
    player_goalie_tex_team_b_ = Texture2D{};
    player_tex_ready_ = false;
}

void detail::AppContext::load_player_team_textures() {
    unload_player_team_textures_if_any();

    bool const skater_ok = load_team_pair("sprites/player_skater.png", "player_sprite.png",
                                          player_tex_team_a_, player_tex_team_b_);
    bool const goalie_ok = load_team_pair("sprites/player_goalie.png", "sprites/player_skater.png",
                                          player_goalie_tex_team_a_, player_goalie_tex_team_b_);

    player_tex_ready_ = skater_ok;
    if (!skater_ok) {
        TraceLog(LOG_INFO,
                 "[zh] Skater sprites missing — circle fallback (resources/sprites/player_skater.png).");
    }
    if (!goalie_ok) {
        TraceLog(LOG_INFO,
                 "[zh] Goalie sprites missing — skater art used for goalies until goalie PNG added.");
        player_goalie_tex_team_a_ = player_tex_team_a_;
        player_goalie_tex_team_b_ = player_tex_team_b_;
    }
}

}  // namespace zh
