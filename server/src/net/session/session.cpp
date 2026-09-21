#include "session.hpp"

#include <spdlog/spdlog.h>
#include <sys/types.h>

#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "aliases/beast_aliases.hpp"
#include "protocol/server_packet.hpp"
#include "server/server.hpp"

namespace lit::net {
Session::Session(std::shared_ptr<Server> server, std::shared_ptr<ISocket> socket, std::size_t id)
    : server_(server),
      socket_(socket),
      id_(id),
      sending_(ATOMIC_FLAG_INIT),
      state_(State::Connecting) {}

void Session::run() {
    spdlog::info("Session::Accept");
    // TODO set timeout
    // TODO set decorator

    auto self = shared_from_this();
    // Accept websocket handshake
    socket_->async_accept([self](const beast::error_code& ec) {
        if (ec) {
            if (ec == websocket::error::closed)
                spdlog::warn("WebSocket was closed cleanly");
            else
                spdlog::error("Accept error: {}", ec.what());

            self->set_disconnecting();
        } else {
            self->set_connected();
        }

        self->process_state();
    });
}

void Session::process_state() {
    switch (get_state()) {
        case State::Connecting:
            break;
        case State::Connected:
            // Add client to server and game
            server_->add_session(shared_from_this());

            // Start reading client inputs
            read_packet_head();
            break;
        case State::Disconnecting:
            socket_->close();
            server_->close_session(id_);
            set_disconnected();
            break;
        case State::Disconnected:
            break;
        case State::User:
            break;
        default:
            spdlog::error("Unknown session state");
    }
}

void Session::read_packet_head() {
    spdlog::info("Session::ReadPacketHead");
    auto self = shared_from_this();
    socket_->async_read_some(packet_handler_.head_current_data(), packet_handler_.head_size_left(),
                             [self](const beast::error_code& ec, std::size_t size) {
                                 // an error occured
                                 if (ec) {
                                     // client close connection
                                     if (ec == websocket::error::closed)
                                         spdlog::warn("WebSocket was closed cleanly");
                                     else
                                         spdlog::error("Read header error: {}", ec.what());

                                     // Close session process
                                     self->set_disconnecting();
                                     self->process_state();
                                     return;
                                 }

                                 // Continue reading the header, untill the complete PacketHead is
                                 // recived.
                                 if (!self->packet_handler_.update_head_size(size))
                                     return self->read_packet_head();

                                 // Check if packet contains payload data, then read the data
                                 if (self->packet_handler_.body_size_left())
                                     return self->read_packet_body();

                                 // Packet does not contain payload data, push to handler
                                 auto packet = std::make_unique<ServerPacket>(
                                     self->packet_handler_.extract_packet(), self->id_);
                                 self->server_->push_packet(std::move(packet));

                                 // Continue reading next packet
                                 self->read_packet_head();
                             });
}

void Session::read_packet_body() {
    spdlog::info("Session::ReadPacketBody");
    auto self = shared_from_this();
    socket_->async_read_some(packet_handler_.body_current_data(), packet_handler_.body_size_left(),
                             [self](const beast::error_code& ec, std::size_t size) {
                                 // an error occured
                                 if (ec) {
                                     // client close connection
                                     if (ec == websocket::error::closed)
                                         spdlog::warn("WebSocket was closed cleanly");
                                     else
                                         spdlog::error("Read body error: {}", ec.what());

                                     // Close session process
                                     self->set_disconnecting();
                                     self->process_state();
                                     return;
                                 }

                                 // Continue reading payload data untill all data has been received.
                                 if (!self->packet_handler_.update_body_size(size))
                                     return self->read_packet_body();

                                 // All payload data has been received, push to handler
                                 auto packet = std::make_unique<ServerPacket>(
                                     self->packet_handler_.extract_packet(), self->id_);
                                 self->server_->push_packet(std::move(packet));

                                 // Continue reading next packet
                                 self->read_packet_head();
                             });
}

void Session::push_to_send(SendBuffer packet) {
    out_queue_.push(packet);
    send();
}

void Session::send() {
    // Check if session is disconnected
    if (!is_connected()) return spdlog::info("Return from send operation, client is disconneted");

    // Check if send queue is empty
    if (out_queue_.empty()) return spdlog::info("Return from send, queue is empty");

    // Check if previous sending finished
    if (start_sending())
        return spdlog::info("Return from send operation, previous send is not finished");

    auto buf = out_queue_.try_pop();
    // This check should never pass
    if (!buf) return spdlog::error("buffer is nullopt:\nfile: {} line: {}", __FILE__, __LINE__);

    auto self = shared_from_this();
    socket_->async_write(
        (*buf)->data(), (*buf)->size(),
        [self, buf](const beast::error_code& ec, [[maybe_unused]] std::size_t size) {
            // an error occured
            if (ec) {
                // client close connection
                if (ec == websocket::error::closed)
                    spdlog::warn("WebSocket was closed cleanly");
                else
                    spdlog::error("Write error: {}", ec.what());

                // Close session process
                self->set_disconnecting();
                self->process_state();
                return;
            }
            // spdlog::info("write {} bytes to client", size);

            self->sending_.clear();
            // If out queue is not empty send again
            if (!self->out_queue_.empty()) self->send();
        });
}
}  // namespace lit::net
