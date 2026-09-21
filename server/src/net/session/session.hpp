#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "packet_handler.hpp"
#include "socket/i_socket.hpp"
#include "utils/ts_queue.hpp"

namespace lit::net {
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

    explicit Session(std::shared_ptr<Server> server, std::shared_ptr<ISocket> socket,
                     std::size_t id);
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void run();
    std::size_t get_id() const { return id_; }
    void push_to_send(SendBuffer packet);

  private:
    // main funciton processing session input state
    void process_state();

    // Read bytes untill read the full packet header
    void read_packet_head();
    void on_read_packet_head(std::size_t size);

    // Read bytes untill read the full payload data
    // and push to incoming queue
    void read_packet_body();
    void on_read_packet_body(std::size_t size);

    void send();

    // Session state
    void set_connecting() const noexcept {
        return state_.store(State::Connecting, std::memory_order_release);
    }
    void set_connected() const noexcept {
        return state_.store(State::Connected, std::memory_order_release);
    }
    void set_disconnecting() const noexcept {
        return state_.store(State::Disconnecting, std::memory_order_release);
    }
    void set_disconnected() const noexcept {
        return state_.store(State::Disconnected, std::memory_order_release);
    }
    void set_user() const noexcept { return state_.store(State::User, std::memory_order_release); }

    [[nodiscard]] State get_state() const noexcept {
        return state_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool is_connected() const noexcept {
        return state_.load(std::memory_order_acquire) != State::Disconnected;
    }

    // Sending state
    [[maybe_unused]] bool start_sending() const noexcept { return sending_.test_and_set(); }
    void stop_sending() const noexcept { return sending_.clear(); }

    std::shared_ptr<Server> server_;
    std::shared_ptr<ISocket> socket_;
    std::size_t id_;
    // Flag indicate session sending packet state: sending/not
    mutable std::atomic_flag sending_;
    mutable std::atomic<State> state_;
    PacketHandler packet_handler_;
    TSQueue<SendBuffer> out_queue_;
};
}  // namespace lit::net
