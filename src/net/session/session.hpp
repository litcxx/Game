#pragma once

#include <memory>

#include "protocol/base_packet.hpp"
#include "socket/i_socket.hpp"

namespace ep::net
{
  class Server;

  class Session : public std::enable_shared_from_this<Session> {
  public:
    explicit Session(std::shared_ptr<Server> server, std::unique_ptr<ISocket> socket);
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Initialie ssl handshake and websocket accept connection
    void Run();

  private:
    // WebSocket accept connection
    void Accept();

    // Read bytes untill read the full packet header
    void ReadPacketHead();

    // Read bytes untill read the full payload data
    // and push to incoming queue
    void ReadPacketBody();

    std::shared_ptr<Server> server_;
    std::unique_ptr<ISocket> socket_;
    std::size_t head_bytes_read_;
    std::size_t body_bytes_read_;
    PacketData packet_;
  };
}
