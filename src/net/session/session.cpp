#include "session.hpp"
#include "protocol/base_packet.hpp"

#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/error.hpp>
#include <cstdint>
#include <spdlog/spdlog.h>

#include "server/server.hpp"
#include "utils/asio_aliases.hpp"
#include "utils/beast_aliases.hpp"

namespace ep::net
{
  Session::Session(std::shared_ptr<Server> server, std::unique_ptr<ISocket> socket) :
    server_{server},
    socket_{std::move(socket)},
    head_bytes_read_{0},
    body_bytes_read_{0}
  {}

  void Session::Run()
  {
    spdlog::info("Session::Run");

    socket_->async_handshake(
      [this](const beast::error_code& ec)
      {
        if (ec)
          return spdlog::error("handshake: {}", ec.what());
        Accept();
      }
    );
  }

  void Session::Accept()
  {

    spdlog::info("Session::Accept");
    // TODO set timeout
    // TODO set decorator

    socket_->async_accept(
      [this](const beast::error_code &ec)
      {
        // client close connection
        if (ec == websocket::error::closed)
          return spdlog::info("session was closed");
        // an error occured
        if (ec)
          return spdlog::error("accept: {}", ec.what());
        ReadPacketHead();
      }
    );
  }

  void Session::ReadPacketHead()
  {
    spdlog::info("Session::ReadPacketHead");
    auto self = shared_from_this();
    socket_->async_read_some(
      reinterpret_cast<uint8_t*>(&packet_.head_) + head_bytes_read_,
      sizeof(PacketHead) - head_bytes_read_,
      [self](const beast::error_code& ec, std::size_t size)
      {
        // client close connection
        if (ec == websocket::error::closed)
          return spdlog::info("session was closed");
        // an error occured
        if (ec)
          return spdlog::error("read: {}", ec.what());

        spdlog::info("read: {} bytes from clients to header", size);

        // Update already read packet header data
        self->head_bytes_read_ += size;

        // The full packet header was read
        if (self->head_bytes_read_ == sizeof(PacketHead)) {
          spdlog::info("The full packet header was read");
          spdlog::info("opcode: {}", self->packet_.head_.opcode_);
          spdlog::info("size: {}", self->packet_.head_.size_);
              
          // Verify the validity of the header
          if (!self->packet_.head_.IsValid()) {
            spdlog::warn("recv invalid header from ip: {}",
                         self->socket_->string_address());
            self->head_bytes_read_ = 0;
            return self->server_->CloseSession(self);
          }

          if (self->packet_.head_.size_ > 0) {
            // Resize body buffer to fit payload as indicated by header
            self->packet_.body_.resize(self->packet_.head_.size_);

            self->ReadPacketBody();
          } else {
            self->head_bytes_read_ = 0;
            self->ReadPacketHead();
          }

        } else {
          self->ReadPacketHead();
        }
      }
    );
  }

  void Session::ReadPacketBody()
  {
    spdlog::info("Session::ReadPacketBody");
    auto self = shared_from_this();
    socket_->async_read_some(
      packet_.body_.data() + body_bytes_read_,
      packet_.body_.size() - body_bytes_read_,
      [self](const beast::error_code& ec, std::size_t size)
      {
        // client close connection
        if (ec == websocket::error::closed)
          return spdlog::info("session was closed");
        // an error occured
        if (ec)
          return spdlog::error("read: {}", ec.what());

        spdlog::info("read: {} bytes from clients", size);

        // Update already read payload data
        self->body_bytes_read_ += size;

        // The full payload data was read
        if (self->body_bytes_read_ == self->packet_.body_.size()) {
          spdlog::info("The full payload data was read");
          // TODO push to incoming queue

          // Reset packet info
          self->head_bytes_read_ = 0;
          self->body_bytes_read_ = 0;
          self->packet_.body_.clear();

          self->ReadPacketHead();
        } else {
          self->ReadPacketBody();
        }
      }
    );
  }
}
