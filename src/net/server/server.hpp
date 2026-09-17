#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "aliases/asio_aliases.hpp"
#include "protocol/server_packet.hpp"
#include "subsystems/game_subsystem.hpp"
#include "subsystems/network_subsystem.hpp"

namespace ep::net {
class Session;

class Server : public std::enable_shared_from_this<Server> {
  public:
    explicit Server(net::io_context& ioc, ssl::context& ctx,
                    std::shared_ptr<NetworkSubsystem> net_susbsystem,
                    std::shared_ptr<GameSubsystem> game_subsystem) noexcept;
    ~Server() = default;

    bool start_listen(Tcp::endpoint endpoint) noexcept;
    // Async accept new client.
    void run();

    void push_packet(std::unique_ptr<ServerPacket> packet) noexcept;
    void add_session(std::shared_ptr<Session> session) noexcept;
    void close_session(std::size_t id);
    void sender();

  private:
    net::io_context& ioc_;
    ssl::context& ctx_;
    Tcp::acceptor acceptor_;
    mutable std::mutex sessions_mutex_;
    std::atomic<std::size_t> new_session_id_;
    std::unordered_map<std::size_t, std::shared_ptr<Session>> sessions_;
    std::shared_ptr<NetworkSubsystem> net_susbsystem_;
    std::shared_ptr<GameSubsystem> game_susbsystem_;
};
}  // namespace ep::net
