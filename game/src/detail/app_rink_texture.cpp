// Playing-scene init (map shapes are drawn procedurally from MapDefinition).
#include "detail/app_context.hpp"

#include "zh/ui/playing_scene_3d.hpp"

namespace zh {

void detail::AppContext::unload_rink_texture_if_any() {
    playing_scene_3d_.unload();
}

void detail::AppContext::load_rink_texture() {
    unload_rink_texture_if_any();
    playing_scene_3d_.ensure_ready();
}

}  // namespace zh
