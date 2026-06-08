// ENet init + ListenHost / GameClient wrappers used by host and WAN client paths.
#include "zh/win32_fix_net_order.h"

#include "zh/network.hpp"

#include <cstdio>

namespace zh::net {

namespace {

constexpr std::size_t kChannels = 2;

bool enet_init_attempted = false;
bool enet_init_ok = false;

[[nodiscard]] bool ensure_enet_initialized_for_ops() {
    if (!enet_init_attempted) {
        enet_init_attempted = true;
        enet_init_ok = enet_initialize() == 0;
        if (!enet_init_ok) {
            std::fputs("[zh] enet_initialize failed.\n", stderr);
        }
    }
    return enet_init_ok;
}

// We always destroy RECEIVE packets after handling — copy bytes first if you need them.
void destroy_packet_if_any(ENetEvent &ev) {
    if (ev.type == ENET_EVENT_TYPE_RECEIVE && ev.packet != nullptr) {
        enet_packet_destroy(ev.packet);
        ev.packet = nullptr;
    }
}

}  // namespace

[[nodiscard]] bool init_library() {
    return ensure_enet_initialized_for_ops();
}

void shutdown_library() {
    if (enet_init_ok) {
        enet_deinitialize();
    }
    enet_init_ok = false;
    enet_init_attempted = false;
}

// --- ListenHost ---

struct ListenHost::Impl {
    ENetHost *host_{nullptr};
};

ListenHost::ListenHost(std::uint16_t port, std::size_t max_sessions)
    : impl_(std::make_unique<Impl>()), bind_port_(port) {
    if (!ensure_enet_initialized_for_ops()) {
        return;
    }
    ENetAddress addr{};
    addr.host = ENET_HOST_ANY;
    addr.port = port;

    constexpr std::size_t kConnectivityHeadroom = 4;
    std::size_t const peer_budget = max_sessions + kConnectivityHeadroom;
    impl_->host_ =
        enet_host_create(&addr, peer_budget, kChannels,
                         static_cast<enet_uint32>(0),
                         static_cast<enet_uint32>(0));
}

ListenHost::~ListenHost() {
    if (impl_ != nullptr && impl_->host_ != nullptr) {
        enet_host_destroy(impl_->host_);
        impl_->host_ = nullptr;
    }
}

bool ListenHost::valid() const {
    return impl_ != nullptr && impl_->host_ != nullptr;
}

void ListenHost::service(std::uint32_t timeout_ms) {
    service({}, timeout_ms);
}

void ListenHost::service(std::function<void(ENetEvent const &)> const &on_event,
                         std::uint32_t timeout_ms) {
    if (!valid()) return;

    // First poll honors timeout_ms; drain the rest of the queue with zero wait.
    bool first_poll = true;
    for (;;) {
        ENetEvent ev{};
        enet_uint32 const to_wait =
            static_cast<enet_uint32>(first_poll ? timeout_ms : 0U);
        first_poll = false;
        int const r = enet_host_service(impl_->host_, &ev, to_wait);
        if (r <= 0) {
            break;
        }
        if (on_event) {
            on_event(ev);
        }
        destroy_packet_if_any(ev);
    }
}

void ListenHost::broadcast_packet(std::span<std::byte const> payload,
                                  std::size_t channel_index,
                                  enet_uint32 flags) {
    if (!valid() || payload.empty()) return;

    ENetPacket *const packet = enet_packet_create(
        payload.data(), static_cast<std::size_t>(payload.size()), flags);
    if (packet == nullptr) return;

    enet_host_broadcast(impl_->host_, static_cast<enet_uint8>(channel_index),
                        packet);
}

void ListenHost::send_to_peer(void *enet_peer_opaque, std::span<std::byte const> payload,
                              std::size_t channel_index, enet_uint32 flags) {
    if (!valid() || enet_peer_opaque == nullptr || payload.empty()) return;

    auto *peer = static_cast<ENetPeer *>(enet_peer_opaque);

    ENetPacket *const packet = enet_packet_create(
        payload.data(), static_cast<std::size_t>(payload.size()), flags);
    if (packet == nullptr) return;

    if (enet_peer_send(peer,
                       static_cast<enet_uint8>(channel_index),
                       packet) != 0) {
        enet_packet_destroy(packet);
    }
}

// --- GameClient ---

struct GameClient::Impl {
    ENetHost *local_{nullptr};
    ENetPeer *server_{nullptr};
};

GameClient::GameClient() : impl_(std::make_unique<Impl>()) {
    if (!ensure_enet_initialized_for_ops()) {
        return;
    }
    constexpr int kOutboundPeers = 1;
    impl_->local_ =
        enet_host_create(nullptr, kOutboundPeers, kChannels,
                         static_cast<enet_uint32>(0),
                         static_cast<enet_uint32>(0));
}

GameClient::~GameClient() {
    disconnect_server();

    if (impl_ != nullptr && impl_->local_ != nullptr) {
        enet_host_destroy(impl_->local_);
        impl_->local_ = nullptr;
    }
}

bool GameClient::valid() const {
    return impl_ != nullptr && impl_->local_ != nullptr;
}

bool GameClient::has_server_peer() const {
    return impl_ != nullptr && impl_->server_ != nullptr;
}

bool GameClient::connect_ipv4(std::string const &ipv4, std::uint16_t port) {
    if (!valid()) return false;
    disconnect_server();

    ENetAddress addr{};
    if (enet_address_set_host(&addr, ipv4.c_str()) != 0) {
        return false;
    }
    addr.port = port;

    impl_->server_ =
        enet_host_connect(impl_->local_, &addr, kChannels,
                          static_cast<enet_uint32>(0));

    return impl_->server_ != nullptr;
}

void GameClient::disconnect_server() {
    if (impl_ != nullptr && impl_->server_ != nullptr) {
        enet_peer_disconnect(impl_->server_, static_cast<enet_uint32>(0));
        impl_->server_ = nullptr;
    }

    if (impl_ != nullptr && impl_->local_ != nullptr) {
        // Brief drain so disconnect packets actually leave the socket.
        bool first_poll = true;
        for (;;) {
            ENetEvent ev{};
            enet_uint32 const to_wait =
                static_cast<enet_uint32>(first_poll ? 50U : 0U);
            first_poll = false;
            int const r = enet_host_service(impl_->local_, &ev, to_wait);
            if (r <= 0) {
                break;
            }
            destroy_packet_if_any(ev);
        }
    }
}

void GameClient::service(std::uint32_t timeout_ms) {
    service({}, timeout_ms);
}

void GameClient::service(std::function<void(ENetEvent const &)> const &on_event,
                         std::uint32_t timeout_ms) {
    if (!valid()) return;

    bool first_poll = true;
    for (;;) {
        ENetEvent ev{};
        enet_uint32 const to_wait =
            static_cast<enet_uint32>(first_poll ? timeout_ms : 0U);
        first_poll = false;
        int const r = enet_host_service(impl_->local_, &ev, to_wait);
        if (r <= 0) {
            break;
        }
        if (on_event) {
            on_event(ev);
        }
        if (ev.type == ENET_EVENT_TYPE_DISCONNECT) {
            impl_->server_ = nullptr;
        }
        destroy_packet_if_any(ev);
    }
}

void GameClient::send_packet(std::span<std::byte const> payload,
                              std::size_t channel_index,
                              enet_uint32 flags) {
    if (!has_server_peer() || payload.empty()) return;

    ENetPacket *const packet = enet_packet_create(
        payload.data(), static_cast<std::size_t>(payload.size()), flags);

    if (packet == nullptr) return;

    if (enet_peer_send(impl_->server_,
                       static_cast<enet_uint8>(channel_index),
                       packet) != 0) {
        enet_packet_destroy(packet);
    }
}

}  // namespace zh::net
