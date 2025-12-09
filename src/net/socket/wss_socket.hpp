#pragma once

#include <atomic>
#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include "boost/asio/ssl/stream.hpp"
#include <boost/beast/websocket/stream.hpp>

#include "aliases/asio_aliases.hpp"
#include "aliases/beast_aliases.hpp"
#include "socket/i_socket.hpp"

namespace ep::net
{
  class WSSSocket : public ISocket, public std::enable_shared_from_this<WSSSocket> {
  public:
    explicit WSSSocket(tcp::socket&& socket, ssl::context& ctx);
    WSSSocket(const WSSSocket&) = delete;
    WSSSocket& operator=(WSSSocket&) = delete;
    ~WSSSocket() = default;

    void async_accept(CompletionHandler handler) override;
    void async_read_some(std::uint8_t* buffer, std::size_t limit, ReadHandler handler) override;
    void async_write(const std::uint8_t* buffer, std::size_t limit, ReadHandler handler) override;

    void close() override;
    std::string string_address() override;

  private:
    void Cancel();
    void CloseWebSocket();
    void CloseSSL();

    [[maybe_unused]] bool IsClosed() const noexcept { return closed_.test_and_set(); }

    websocket::stream<ssl::stream<tcp::socket>> socket_;
    // Flag indicate socket closed state: true / false
    mutable std::atomic_flag closed_;
  };
}
