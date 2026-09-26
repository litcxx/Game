#include "server.hpp"

#include <spdlog/spdlog.h>

#include <boost/asio/experimental/awaitable_operators.hpp>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config/config.hpp"
#include "session/session.hpp"

namespace ip = asio::ip;

namespace lit::net {
Server::Server(asio::io_context& io, const NetConfig& config, TSQueue<ClientEvent>& incoming_events)
    : io_{io},
      acceptor_{io},
      acceptor_strand_{asio::make_strand(io)},
      config_{config},
      incoming_events_{incoming_events} {
    const ip::address address = ip::make_address(config_.ip);
    const std::uint16_t port = config_.port;
    const tcp::endpoint endpoint{address, port};
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections);
}

Server::~Server() = default;

void Server::listen() { asio::co_spawn(acceptor_strand_, do_listen(), asio::detached); }

asio::awaitable<void> Server::do_listen() {
    for (;;) {
        auto [ec, socket] = co_await acceptor_.async_accept(asio::make_strand(io_),
                                                            asio::as_tuple(asio::use_awaitable));
        if (ec) {
            // acceptor_.close() during graceful shutdown cancels the pending accept.
            if (ec != asio::error::operation_aborted) {
                spdlog::error("Server::do_listen accept error: {}", ec.message());
            }
            break;
        }

        auto ws = Socket(std::move(socket));
        // Capture the executor before moving `ws` into add_session (function-argument
        // evaluation order is unspecified).
        auto executor = ws.get_executor();
        asio::co_spawn(executor, add_session(std::move(ws)), asio::detached);
    }
}

void Server::stop() {
    // Stop accepting (on the acceptor strand), then ask each session to close on
    // its own strand. Closing a socket ends its do_read; the `||` cancels do_send;
    // run_session then erases the session. Once all coroutines finish and no work
    // remains, io.run() returns on its own.
    asio::post(acceptor_strand_, [this] {
        boost::system::error_code ec;
        acceptor_.close(ec);
    });

    std::vector<std::pair<std::uint64_t, asio::any_io_executor>> targets;
    {
        std::lock_guard lock(sessions_mutex_);
        targets.reserve(sessions_.size());
        for (auto& [id, session] : sessions_) {
            targets.emplace_back(id, session.executor());
        }
    }
    for (auto& [id, ex] : targets) {
        asio::post(ex, [this, id] {
            std::lock_guard lock(sessions_mutex_);
            auto it = sessions_.find(id);
            if (it != sessions_.end()) {
                it->second.close();
            }
        });
    }
}

asio::awaitable<void> Server::add_session(Socket socket) {
    const std::uint64_t id = next_sessions_id_.fetch_add(1);
    auto executor = socket.get_executor();
    try {
        // WebSocket handshake
        co_await socket.async_accept(asio::use_awaitable);

        // Store the session by value in the map. Node addresses are stable, so
        // references survive other insert/erase and rehash (only erasing this very
        // entry invalidates them) — this is what makes by-value storage safe.
        {
            std::lock_guard lock(sessions_mutex_);
            auto [it, success] = sessions_.try_emplace(id, *this, std::move(socket), id);
            assert(success);
        }

        // One supervisor per session runs both I/O coroutines and removes the
        // session only after BOTH have finished (see run_session).
        co_spawn(executor, run_session(id), asio::detached);
    } catch (const std::exception& e) {
        spdlog::error("Server::add_session error id={}: {}", id, e.what());
    }
}

asio::awaitable<void> Server::run_session(std::uint64_t id) {
    using namespace asio::experimental::awaitable_operators;

    Session* session = nullptr;
    {
        std::lock_guard lock(sessions_mutex_);
        auto it = sessions_.find(id);
        if (it == sessions_.end()) co_return;
        session = &it->second;  // stays valid until this id is erased below
    }

    try {
        // Resumes only after BOTH coroutines finish: when one returns (EOF/error),
        // `||` cancels the other and waits for it to unwind. After this line no
        // coroutine frame holds `session`, so the erase below is safe.
        co_await (session->do_read() || session->do_send());
    } catch (const std::exception& e) {
        spdlog::warn("Server::run_session id={} ended with exception: {}", id, e.what());
    }

    session->close();  // session still alive here; releases the OS socket

    {
        std::lock_guard lock(sessions_mutex_);
        sessions_.erase(id);
        spdlog::info("Server::run_session id={} removed, sessions={}", id, sessions_.size());
    }

    // Tell the game loop the player is gone (processed after this session's
    // earlier messages, which are already ahead in the queue).
    incoming_events_.push(ClientEvent{id, ClientEvent::Kind::Disconnected, {}});
}

void Server::push_packet(std::uint64_t session_id, ::game::v1::ClientMessage packet) {
    incoming_events_.push(ClientEvent{session_id, ClientEvent::Kind::Message, std::move(packet)});
}

void Server::send_to(std::uint64_t session_id, std::vector<std::byte> bytes) {
    std::lock_guard lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.try_send(std::move(bytes));
    }
}

void Server::broadcast(std::vector<std::byte> bytes) {
    std::lock_guard lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        session.try_send(bytes);  // one copy per session
    }
}

void Server::close_session(std::size_t id) {
    // Grab the session's executor under the lock, then ask it to close on its own
    // strand. Closing the socket unblocks do_read/do_send; the supervisor
    // (run_session) then erases the entry. We never erase here — run_session is
    // the single eraser.
    asio::any_io_executor ex;
    {
        std::lock_guard lock(sessions_mutex_);
        auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return;
        }
        ex = it->second.executor();
    }
    asio::post(ex, [this, id] {
        std::lock_guard lock(sessions_mutex_);
        auto it = sessions_.find(id);
        if (it != sessions_.end()) {
            it->second.close();
        }
    });
}
}  // namespace lit::net
