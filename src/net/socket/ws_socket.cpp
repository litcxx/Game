#include "ws_socket.hpp"

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
  WSSocket::WSSocket(tcp::socket&& socket) :
    socket_(std::move(socket)),
    closed_(ATOMIC_FLAG_INIT)
  {
    socket_.binary(true);
    spdlog::info("Set websocket binary mode: {}", socket_.binary());
  }

  void WSSocket::close()
  {
    // Check if already closed
    if (IsClosed()) return;
    spdlog::info("Close connection");
    Cancel();
    CloseWebSocket();
  }

  void WSSocket::Cancel()
  {
    // Cancel all async operations
    boost::system::error_code ec;
    if (socket_.next_layer().is_open()) {
      socket_.next_layer().cancel(ec);
      if (ec)
        spdlog::warn("Cancel error: {}", ec.what());
    }
  }

  void WSSocket::CloseWebSocket()
  {
    auto self = shared_from_this();
    socket_.async_close(
      beast::websocket::normal,
      [self](const beast::error_code& ec)
      {
        if (ec)
          spdlog::warn("Close websocket error: {}", ec.what());
      }
    );
  }

  void WSSocket::async_accept(CompletionHandler handler)
  {
    socket_.async_accept(std::move(handler));
  }

  void WSSocket::async_read_some(std::uint8_t* buffer, std::size_t limit, ReadHandler handler)
  {
    socket_.async_read_some(net::buffer(buffer, limit), std::move(handler));
  }

  void WSSocket::async_write(const std::uint8_t* buffer, std::size_t limit, ReadHandler handler)
  {
    socket_.async_write(net::buffer(buffer, limit), handler);
  }

  std::string WSSocket::string_address()
  {
    return socket_.next_layer().remote_endpoint().address().to_string();
  }
}
