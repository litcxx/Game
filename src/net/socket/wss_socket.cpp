#include "wss_socket.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <spdlog/spdlog.h>

namespace ep::net
{
  WSSSocket::WSSSocket(tcp::socket&& socket, ssl::context& ctx) :
    socket_(std::move(socket), ctx)
  {}

  WSSSocket::~WSSSocket()
  {
    close();
  }

  void WSSSocket::close()
  {
    beast::error_code ec;
    socket_.close(websocket::close_code::normal, ec);
    spdlog::info("close websocket: {}", ec ? ec.what() : "success");
  }

  void WSSSocket::async_read_some(uint8_t* buffer, std::size_t limit, ReadHandler handler)
  {
    socket_.async_read_some(net::buffer(buffer, limit), std::move(handler));
  }

  void WSSSocket::async_accept(CompletionHandler handler)
  {
    socket_.async_accept(std::move(handler));
  }

  void WSSSocket::async_handshake(CompletionHandler handler)
  {
    socket_.next_layer().async_handshake(ssl::stream_base::server, std::move(handler));
  }

  std::string WSSSocket::string_address()
  {
    return socket_.next_layer().next_layer().remote_endpoint().address().to_string();
  }
}
