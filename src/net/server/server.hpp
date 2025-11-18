#pragma once

#include <memory>
#include <list>
#include <mutex>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include "aliases/asio_aliases.hpp"
#include "protocol/base_packet.hpp"
#include "protocol/network_subsystem.hpp"

namespace ep::net
{
  class Session;

  class Server : public std::enable_shared_from_this<Server> {
  public:
    explicit Server(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint, std::shared_ptr<NetworkSubsystem> net_susbsystem);
    ~Server() = default;

    // Async accept new client.
    void Run();

    void PushPacket(NetPacket packet, std::size_t id);

    void CloseSession(std::shared_ptr<Session> session);
  private:
    void AddSession(std::shared_ptr<Session> session) noexcept;

    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
    mutable std::mutex sessions_mutex_;
    std::atomic<std::size_t> new_session_id_;
    std::list<std::shared_ptr<Session>> sessions_;
    std::shared_ptr<NetworkSubsystem> net_susbsystem_;
  };
}
