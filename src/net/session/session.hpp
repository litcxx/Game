#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <atomic>

#include "packet_handler.hpp"
#include "socket/i_socket.hpp"
#include "utils/ts_queue.hpp"

namespace ep::net
{
  class Server;

  class Session : public std::enable_shared_from_this<Session> {
  public:
    using SendBuffer = std::shared_ptr<std::vector<std::uint8_t>>;

    explicit Session(std::shared_ptr<Server> server, std::unique_ptr<ISocket> socket, std::size_t id);
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Initialie ssl handshake and websocket accept connection
    void Run();
    void Send();
    std::size_t GetID() const { return id_; }

    // Session state
    [[maybe_unused]] bool SetConnected() const noexcept { return state_.test_and_set(); }
    void SetDisconneted() const noexcept { state_.clear(); }
    // true - connected, false - disconnected
    [[nodiscard]] bool GetState() const noexcept { return state_.test(); }

    // true - sending, false - not sending
    [[maybe_unused]] bool StartSending() const noexcept { return sending_.test_and_set(); }
    void StopSending() const noexcept { return sending_.clear(); }

    void PushToSend(SendBuffer packet);

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
    mutable std::atomic_flag state_;
    PacketHandler packet_handler_;
    mutable std::atomic_flag sending_;
    ep::TSQueue<std::vector<uint8_t>> out_queue_;
  };
}
