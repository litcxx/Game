#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include "utils/asio_aliases.hpp"
#include "utils/beast_aliases.hpp"

class Session : public std::enable_shared_from_this<Session> {
public:
  explicit Session(tcp::socket&& socket);

  void Run();

private:
  void ReadRequest();
  void SendData();

  websocket::stream<tcp::socket> websocket_;
  beast::flat_buffer buffer_;
};
