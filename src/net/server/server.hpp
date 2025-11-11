#pragma once

#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include "utils/asio_aliases.hpp"

namespace ep::net
{
  class Server : public std::enable_shared_from_this<Server> {
  public:
    explicit Server(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint);

    void Run();
  private:

    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
  };
}
