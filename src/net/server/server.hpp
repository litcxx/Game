#pragma once

#include <memory>
#include <list>
#include <mutex>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include "utils/asio_aliases.hpp"
#include "utils/ts_queue.hpp"

namespace ep::net
{
  class Session;

  class Server : public std::enable_shared_from_this<Server> {
  public:
    explicit Server(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint);
    ~Server() = default;

    void Run();
    void CloseSession(std::shared_ptr<Session> session);
  private:
    void AddSession(std::shared_ptr<Session> session) noexcept;

    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
    mutable std::mutex sessions_mutex_;
    std::list<std::shared_ptr<Session>> sessions_;
  };
}
