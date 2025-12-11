#include "wss_socket.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/system/detail/error_code.hpp>

#include <cstdint>
#include <spdlog/spdlog.h>

namespace ep::net
{
  WSSSocket::WSSSocket(tcp::socket&& socket, ssl::context& ctx) :
    socket_(std::move(socket), ctx),
    closed_(ATOMIC_FLAG_INIT)
  {
    socket_.binary(true);
    spdlog::info("Set websocket binary mode: {}", socket_.binary());
  }

  void WSSSocket::close()
  {
    // Check if already closed
    if (IsClosed()) return;
    spdlog::info("WSSSocket::close");
    Cancel();
    CloseWebSocket();
    CloseSSL();
  }

  void WSSSocket::Cancel()
  {
    // Cancel all async operations
    boost::system::error_code ec;
    if (socket_.next_layer().next_layer().is_open()) {
      socket_.next_layer().next_layer().cancel(ec);
      if (ec)
        spdlog::warn("Cancel error: {}", ec.what());
    }
  }

  void WSSSocket::CloseWebSocket()
  {
    auto self = shared_from_this();
    socket_.async_close(
      beast::websocket::normal,
      [self](const beast::error_code& ec)
      {
        if (ec)
          spdlog::warn("Close websocket error: {}", ec.what());
        
        self->CloseSSL();
      }
    );
  }

  void WSSSocket::CloseSSL()
  {
    auto self = shared_from_this();
    socket_.next_layer().async_shutdown(
      [self](const boost::system::error_code& ec)
      {
        if (ec)
          spdlog::warn("Shutdown ssl error: {}", ec.what());
      }
    );
  }

  void WSSSocket::async_accept(CompletionHandler handler)
  {
    auto self = shared_from_this();
    socket_.next_layer().async_handshake(
      ssl::stream_base::server,
      [self, handler = std::move(handler)](const beast::error_code& ec)
      {
        if (ec) {
          // client close connection
          if (ec == websocket::error::closed)
            return spdlog::warn("WebSocket was closed cleanly");
          else
            return spdlog::error("SSL handshake error: {}", ec.what());
        }

        self->socket_.async_accept(std::move(handler));
      }
    );
  }

  void WSSSocket::async_read_some(std::uint8_t* buffer, std::size_t limit, ReadHandler handler)
  {
    socket_.async_read_some(net::buffer(buffer, limit), std::move(handler));
  }

  void WSSSocket::async_write(const std::uint8_t* buffer, std::size_t limit, ReadHandler handler)
  {
    socket_.async_write(net::buffer(buffer, limit), handler);
  }

  std::string WSSSocket::string_address()
  {
    return socket_.next_layer().next_layer().remote_endpoint().address().to_string();
  }
}
