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

    explicit Session(std::shared_ptr<Server> server, std::shared_ptr<ISocket> socket, std::size_t id);
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void Run();
    std::size_t GetID() const { return id_; }
    void PushToSend(SendBuffer packet);
  private:

    // Read bytes untill read the full packet header
    void ReadPacketHead();
    void OnReadPacketHead(std::size_t size);

    // Read bytes untill read the full payload data
    // and push to incoming queue
    void ReadPacketBody();
    void OnReadPacketBody(std::size_t size);

    void Send();

    // Session state
    [[maybe_unused]] bool SetConnected() const noexcept { return state_.test_and_set(); }
    void SetDisconneted() const noexcept { state_.clear(); }
    [[nodiscard]] bool IsConnected() const noexcept { return state_.test(); }

    // Sending state
    [[maybe_unused]] bool StartSending() const noexcept { return sending_.test_and_set(); }
    void StopSending() const noexcept { return sending_.clear(); }

    std::shared_ptr<Server> server_;
    std::shared_ptr<ISocket> socket_;
    std::size_t id_;
    // Flag indicate session state: connected/disconnected
    mutable std::atomic_flag state_;
    PacketHandler packet_handler_;
    // Flag indicate session sending packet state: sending/not
    mutable std::atomic_flag sending_;
    ep::TSQueue<std::vector<uint8_t>> out_queue_;
  };
}
