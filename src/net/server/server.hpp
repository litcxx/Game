#pragma once

#include <memory>
#include <list>
#include <mutex>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include "protocol/base_packet.hpp"
#include "aliases/asio_aliases.hpp"
#include "utils/ts_queue.hpp"

namespace ep::net
{
  class Session;

  class Server : public std::enable_shared_from_this<Server> {
  public:
    explicit Server(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint);
    ~Server() = default;

    // Async accept new client.
    void Run();

    void PushPacket(PacketData packet);

    void CloseSession(std::shared_ptr<Session> session);
  private:
    void AddSession(std::shared_ptr<Session> session) noexcept;

    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
    mutable std::mutex sessions_mutex_;
    std::atomic<std::size_t> new_session_id_;
    std::list<std::shared_ptr<Session>> sessions_;
    ep::utils::TSQueue<PacketData> incoming_queue_;
  };
}
