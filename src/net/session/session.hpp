#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include "boost/asio/ssl/stream.hpp"
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include "utils/asio_aliases.hpp"
#include "utils/beast_aliases.hpp"

namespace ep::net
{
  class Session : public std::enable_shared_from_this<Session> {
  public:
    explicit Session(tcp::socket&& socket, ssl::context& ctx);

    void Run();

  private:
    void Accept();
    void ReadRequest();
    void SendData();

    websocket::stream<ssl::stream<tcp::socket>> socket_;
    beast::flat_buffer buffer_;
  };
}

