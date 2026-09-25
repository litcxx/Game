#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/basic_concurrent_channel.hpp>
#include <boost/asio/experimental/channel_traits.hpp>
#include <boost/beast.hpp>
#include <cstddef>
#include <span>
#include <vector>

#include "game/v1/protocol.pb.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using Socket = websocket::stream<beast::tcp_stream>;
using Channel = asio::experimental::basic_concurrent_channel<
    asio::any_io_executor, asio::experimental::channel_traits<>,
    void(boost::system::error_code, std::vector<std::byte>)>;

namespace lit::net {
class Server;

class Session {
  public:
    explicit Session(Server& server, Socket socket, std::size_t id);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() = default;

    asio::awaitable<void> do_read();
    asio::awaitable<void> do_send();

    // Non-blocking enqueue of an outgoing frame; returns false if the send queue
    // is full or closed (the frame is then dropped). Safe to call from any thread.
    bool send(std::vector<std::byte> msg);

    // Best-effort socket close, used by the supervisor during teardown.
    void close() noexcept;

    // Executor (strand) this session's I/O runs on; used to post strand-safe work.
    asio::any_io_executor executor() { return socket_.get_executor(); }

    std::size_t get_id() const noexcept { return id_; }

  private:
    void process_data(std::span<const char> buff);

    Server& server_;
    Socket socket_;
    std::size_t id_;
    Channel send_queue_;
};
}  // namespace lit::net
