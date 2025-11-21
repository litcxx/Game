#pragma once

#include <boost/asio/ip/tcp.hpp>
#include "boost/asio/ssl/stream.hpp"
#include <boost/beast/websocket/stream.hpp>

#include "aliases/asio_aliases.hpp"
#include "aliases/beast_aliases.hpp"
#include "socket/i_socket.hpp"

namespace ep::net
{
  class WSSSocket : public ISocket {
  public:
    explicit WSSSocket(tcp::socket&& socket, ssl::context& ctx);
    WSSSocket(const WSSSocket&) = delete;
    WSSSocket& operator=(WSSSocket&) = delete;
    ~WSSSocket();

    void async_read_some(std::uint8_t* buffer, std::size_t limit, ReadHandler handler) override;
    void async_write(const std::uint8_t* buffer, std::size_t limit, ReadHandler handler) override;
    void async_accept(CompletionHandler handler) override;
    void async_handshake(CompletionHandler handler) override;
    std::string string_address() override;
  private:
    void close() override;

    websocket::stream<ssl::stream<tcp::socket>> socket_;
  };
}

