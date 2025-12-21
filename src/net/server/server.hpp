#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include "aliases/asio_aliases.hpp"
#include "protocol/net_packet.hpp"
#include "subsystems/game_subsystem.hpp"
#include "subsystems/network_subsystem.hpp"

namespace ep::net
{
  class Session;

  class Server : public std::enable_shared_from_this<Server> {
  public:
    explicit Server(net::io_context& ioc, 
                    ssl::context& ctx, 
                    std::shared_ptr<NetworkSubsystem> net_susbsystem,
                    std::shared_ptr<GameSubsystem> game_subsystem) noexcept;
    ~Server() = default;

    bool StartListen(tcp::endpoint endpoint) noexcept;
    // Async accept new client.
    void Run();

    void PushPacket(NetPacket packet, std::size_t id);
    void AddSession(std::shared_ptr<Session> session) noexcept;
    void CloseSession(std::size_t id);
    void Sender();
  private:
    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
    mutable std::mutex sessions_mutex_;
    std::atomic<std::size_t> new_session_id_;
    std::unordered_map<std::size_t, std::shared_ptr<Session>> sessions_;
    std::shared_ptr<NetworkSubsystem> net_susbsystem_;
    std::shared_ptr<GameSubsystem> game_susbsystem_;
  };
}
