#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "packet_handler.hpp"
#include "socket/i_socket.hpp"

namespace ep::net
{
  class Server;

  class Session : public std::enable_shared_from_this<Session> {
  public:
    explicit Session(std::shared_ptr<Server> server, std::unique_ptr<ISocket> socket, std::size_t id);
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Initialie ssl handshake and websocket accept connection
    void Run();
    void Send(std::shared_ptr<std::vector<uint8_t>> buf);
    std::size_t GetID() const { return id_; }

  private:
    // WebSocket accept connection
    void Accept();

    // Read bytes untill read the full packet header
    void ReadPacketHead();
    void OnReadPacketHead(std::size_t size);

    // Read bytes untill read the full payload data
    // and push to incoming queue
    void ReadPacketBody();
    void OnReadPacketBody(std::size_t size);

    std::shared_ptr<Server> server_;
    std::unique_ptr<ISocket> socket_;
    std::size_t id_;
    PacketHandler packet_handler_;
  };
}
