#pragma once

#include <memory>

namespace zh {

namespace detail {
struct AppContext;
}

// Single-game instance: window, screens, host/client session, render loop.
// impl_ is a pimpl so this header never pulls in raylib or ENet.
class App {
public:
    static constexpr float kFixedDt = 1.f / 60.f;  // sim step; render rate is separate

    App();
    ~App();

    App(App const&) = delete;
    App& operator=(App const&) = delete;
    App(App&&) noexcept;
    App& operator=(App&&) noexcept;

    // Blocks until the window closes. Returns process exit code (0 today).
    int run();

private:
    std::unique_ptr<detail::AppContext> impl_;
};

}  // namespace zh
