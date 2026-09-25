#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <mutex>
#include <unordered_map>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "net/client_envelope.hpp"
#include "net/i_client_gateway.hpp"
#include "utils/ts_queue.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using Socket = websocket::stream<beast::tcp_stream>;

namespace lit::net {
class Session;

// Owns the acceptor and the session registry, and serves as the game loop's
// gateway to clients (IClientGateway).
class Server : public IClientGateway {
  public:
    explicit Server(asio::io_context& io, const NetConfig& config,
                    TSQueue<ClientEnvelope>& incoming_msgs);
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    // Defined out-of-line in server.cpp (sessions_ stores Session by value and
    // needs the complete type; main.cpp only sees the forward declaration).
    ~Server() override;

    bool start_listen(tcp::endpoint endpoint) noexcept;
    asio::awaitable<void> do_listen();

    // Called by a session: hand a parsed client message (tagged with its session)
    // to the game loop's inbound queue.
    void push_packet(std::uint64_t session_id, ::game::v1::ClientMessage packet);
    void close_session(std::size_t id);

    // IClientGateway: the game loop hands us serialized frames to deliver.
    void send_to(std::uint64_t session_id, std::vector<std::byte> bytes) override;
    void broadcast(std::vector<std::byte> bytes) override;

  private:
    asio::awaitable<void> add_session(Socket socket);
    // Supervises one session: runs do_read and do_send together and erases the
    // session from the map only after BOTH have finished.
    asio::awaitable<void> run_session(std::uint64_t id);

    // --- NETWORK ---
    asio::io_context& io_;
    tcp::acceptor acceptor_;
    const NetConfig config_;

    // --- SESSIONS ---
    std::unordered_map<std::uint64_t, Session> sessions_;
    std::atomic<std::uint64_t> sessions_id_{0};
    mutable std::mutex sessions_mutex_;

    // --- GAME LOOP INBOUND QUEUE ---
    TSQueue<ClientEnvelope>& incoming_msgs_;
};
}  // namespace lit::net
