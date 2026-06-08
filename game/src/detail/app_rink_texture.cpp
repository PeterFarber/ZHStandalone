// Thin wrapper — rink PNG load lives in zh::ui::load_rink_sprites.
#include "detail/app_context.hpp"

#include "zh/ui/rink_sprites.hpp"

namespace zh {

void detail::AppContext::unload_rink_texture_if_any() {
    zh::ui::unload_rink_sprites(rink_sprites_);
}

void detail::AppContext::load_rink_texture() {
    unload_rink_texture_if_any();
    zh::ui::load_rink_sprites(rink_sprites_);
}

}  // namespace zh
