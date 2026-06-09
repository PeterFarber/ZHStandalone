#pragma once

// Thin RAII around ENet. Two channels: gameplay (unreliable) and handshake (reliable).

#if defined(_WIN32)
#include "zh/winsock_first.h"
#endif

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

#include <enet/enet.h>

#include <memory>
#include <string>

namespace zh::net {

[[nodiscard]] bool init_library();
void shutdown_library();

class ListenHost {
public:
    ListenHost(std::uint16_t port, std::size_t max_sessions = 8);
    ~ListenHost();

    ListenHost(ListenHost const&) = delete;
    ListenHost& operator=(ListenHost const&) = delete;
    ListenHost(ListenHost&&) noexcept = delete;
    ListenHost& operator=(ListenHost&&) noexcept = delete;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] std::uint16_t port() const { return bind_port_; }

    void service(std::uint32_t timeout_ms = 0);
    // Copy payload bytes in on_event — we destroy the ENet packet after the callback.
    void service(std::function<void(ENetEvent const &)> const &on_event,
                 std::uint32_t timeout_ms = 0);

    void broadcast_packet(std::span<std::byte const> payload, std::size_t channel_index,
                          enet_uint32 flags);

    void send_to_peer(void *enet_peer_opaque, std::span<std::byte const> payload,
                      std::size_t channel_index, enet_uint32 flags);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::uint16_t bind_port_{0};
};

class GameClient {
public:
    GameClient();
    ~GameClient();

    GameClient(GameClient const&) = delete;
    GameClient& operator=(GameClient const&) = delete;
    GameClient(GameClient&&) noexcept = delete;
    GameClient& operator=(GameClient&&) noexcept = delete;

    [[nodiscard]] bool valid() const;

    [[nodiscard]] bool connect_ipv4(std::string const& ipv4, std::uint16_t port);
    void disconnect_server();

    void service(std::uint32_t timeout_ms = 0);
    void service(std::function<void(ENetEvent const &)> const &on_event,
                 std::uint32_t timeout_ms = 0);

    void send_packet(std::span<std::byte const> payload, std::size_t channel_index,
                     enet_uint32 flags);

    [[nodiscard]] bool has_server_peer() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace zh::net
