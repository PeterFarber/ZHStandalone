// Public face of the executable. Includes stay light — raylib/enet live in detail::AppContext.
#include "zh/app.hpp"

#include "detail/app_context.hpp"

namespace zh {

App::App() : impl_(std::make_unique<detail::AppContext>()) {}

App::~App() = default;

App::App(App&&) noexcept = default;

App& App::operator=(App&&) noexcept = default;

int App::run() {
    return impl_->run();
}

}  // namespace zh
