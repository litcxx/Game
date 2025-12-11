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

    enum class State : std::uint8_t {
      Connecting,
      Connected,
      Disconnecting,
      Disconnected,
      User,
    };

    explicit Session(std::shared_ptr<Server> server, std::shared_ptr<ISocket> socket, std::size_t id);
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void Run();
    std::size_t GetID() const { return id_; }
    void PushToSend(SendBuffer packet);
  private:

    // main funciton processing session input state
    void ProcessState();

    // Read bytes untill read the full packet header
    void ReadPacketHead();
    void OnReadPacketHead(std::size_t size);

    // Read bytes untill read the full payload data
    // and push to incoming queue
    void ReadPacketBody();
    void OnReadPacketBody(std::size_t size);

    void Send();

    // Session state
    void SetConnecting() const noexcept { return state_.store(State::Connecting, std::memory_order_release); }
    void SetConnected() const noexcept { return state_.store(State::Connected, std::memory_order_release); }
    void SetDisconneting() const noexcept { return state_.store(State::Disconnecting, std::memory_order_release); }
    void SetDisconneted() const noexcept { return state_.store(State::Disconnected, std::memory_order_release); }
    void SetUser() const noexcept { return state_.store(State::User, std::memory_order_release); }

    [[nodiscard]] State GetState() const noexcept { return state_.load(std::memory_order_acquire); }

    [[nodiscard]] bool IsConnected() const noexcept { return state_.load(std::memory_order_acquire) != State::Disconnected; }

    // Sending state
    [[maybe_unused]] bool StartSending() const noexcept { return sending_.test_and_set(); }
    void StopSending() const noexcept { return sending_.clear(); }

    std::shared_ptr<Server> server_;
    std::shared_ptr<ISocket> socket_;
    std::size_t id_;
    // Flag indicate session sending packet state: sending/not
    mutable std::atomic_flag sending_;
    mutable std::atomic<State> state_;  
    PacketHandler packet_handler_;
    ep::TSQueue<std::vector<uint8_t>> out_queue_;
  };
}
