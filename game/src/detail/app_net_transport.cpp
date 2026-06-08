// Start/stop listen host or outbound client. Host and client are never active together.
#include "detail/app_context.hpp"

#include "zh/network.hpp"

namespace zh {

void detail::AppContext::start_listen_server(std::uint16_t port) {
    stop_client();
    stop_listen_server();
    listen_server_ = std::make_unique<net::ListenHost>(port, 8U);
    if (!listen_server_->valid()) {
        listen_server_.reset();
    }
}

void detail::AppContext::stop_listen_server() {
    listen_server_.reset();
}

bool detail::AppContext::start_client_join(std::string const &ipv4, std::uint16_t const port) {
    return wan_.connect_ipv4(ipv4, port);
}

void detail::AppContext::stop_client() {
    wan_.disconnect_transport();
}

}  // namespace zh
