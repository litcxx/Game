#include "session.hpp"

#include <spdlog/spdlog.h>

#include "server/server.hpp"

namespace {
constexpr int kChannelMax = 10;
// Per protocol.proto a client frame larger than ~1 KiB is a protocol error;
// cap reads so an oversized frame can't allocate unbounded memory.
constexpr std::size_t kMaxIncomingMessage = 1024;
}  // namespace

namespace lit::net {
Session::Session(Server& server, Socket socket, std::size_t id)
    : server_(server),
      socket_(std::move(socket)),
      id_(id),
      send_queue_(socket_.get_executor(), kChannelMax) {
    socket_.binary(true);  // protocol uses binary frames only
}

asio::awaitable<void> Session::do_read() {
    spdlog::info("Session::do_read id={}", id_);
    // One WebSocket message == one ClientMessage (see protocol.proto): read whole
    // messages via async_read, no manual header/body framing. Cap the size so an
    // oversized frame can't exhaust memory.
    socket_.read_message_max(kMaxIncomingMessage);

    beast::flat_buffer buffer;
    for (;;) {
        auto [ec, n] = co_await socket_.async_read(buffer, asio::as_tuple(asio::use_awaitable));
        if (ec) {
            // Client closed the connection or a read error occurred: stop reading.
            // The supervisor (Server::run_session) then tears the session down.
            if (ec != websocket::error::closed)
                spdlog::warn("Session::do_read error id={}: {}", id_, ec.message());
            break;
        }

        // The protocol uses binary frames only.
        if (!socket_.got_binary()) {
            spdlog::warn("Session::do_read dropping non-binary frame id={}", id_);
            buffer.consume(buffer.size());
            continue;
        }

        const auto bytes = buffer.data();
        process_data({static_cast<const char*>(bytes.data()), bytes.size()});
        buffer.consume(buffer.size());
    }
}

void Session::process_data(std::span<const char> buff) {
    // Session is pure transport: parse one frame and forward it (tagged with our
    // id) to the game loop. All dispatch and validation live in World.
    ::game::v1::ClientMessage message;
    if (!message.ParseFromArray(buff.data(), static_cast<int>(buff.size()))) {
        spdlog::warn("Session::process_data parse failed id={}", id_);
        return;
    }
    server_.push_packet(id_, std::move(message));
}

asio::awaitable<void> Session::do_send() {
    spdlog::info("Session::do_send id={}", id_);
    for (;;) {
        auto [rec, buff] = co_await send_queue_.async_receive(asio::as_tuple(asio::use_awaitable));
        if (rec) break;  // channel closed or cancelled -> stop.

        auto [wec, n] =
            co_await socket_.async_write(asio::buffer(buff), asio::as_tuple(asio::use_awaitable));
        if (wec) break;  // write failed or cancelled -> stop.
    }
}

bool Session::send(std::vector<std::byte> msg) {
    return send_queue_.try_send(boost::system::error_code{}, std::move(msg));
}

void Session::close() noexcept {
    boost::system::error_code ec;
    beast::get_lowest_layer(socket_).socket().close(ec);
}
}  // namespace lit::net
