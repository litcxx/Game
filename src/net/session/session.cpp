#include "session.hpp"

#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/error.hpp>
#include <cstdint>
#include <spdlog/spdlog.h>

#include "server/server.hpp"
#include "aliases/asio_aliases.hpp"
#include "aliases/beast_aliases.hpp"

namespace ep::net
{
  Session::Session(std::shared_ptr<Server> server, std::unique_ptr<ISocket> socket, std::size_t id) :
    server_{server},
    socket_{std::move(socket)},
    id_(id)
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

        // Push Add player to world packet
        ReadPacketHead();
      }
    );
  }

  void Session::ReadPacketHead()
  {
    spdlog::info("Session::ReadPacketHead");
    auto self = shared_from_this();
    socket_->async_read_some(
      packet_handler_.HeaderData(),
      packet_handler_.HeaderSize(),
      [self](const beast::error_code& ec, std::size_t size)
      {
        // client close connection
        if (ec == websocket::error::closed)
          return spdlog::info("session was closed");
        // an error occured
        if (ec)
          return spdlog::error("read: {}", ec.what());

        // Continue reading the header, untill the complete PacketHead is recived.
        if (!self->packet_handler_.ReadHeader(size))
          return self->ReadPacketHead();
        else {
          // All header has been received. If payload size == 0 push to queue 
          // and continue read next header. Otherwise read the body.
          if (self->packet_handler_.GetBodySize() != 0)
            self->ReadPacketBody();
          else {
            self->server_->PushPacket(std::move(self->packet_handler_.ExtractPacket()), self->id_);
            self->ReadPacketHead();
          }
        }
      }
    );
  }

  void Session::ReadPacketBody()
  {
    spdlog::info("Session::ReadPacketBody");
    auto self = shared_from_this();
    socket_->async_read_some(
      packet_handler_.BodyData(),
      packet_handler_.BodySize(),
      [self](const beast::error_code& ec, std::size_t size)
      {
        // Client close connection.
        if (ec == websocket::error::closed)
          return spdlog::info("session was closed");
        // An error occured.
        if (ec)
          return spdlog::error("read: {}", ec.what());

        // Continue reading payload data untill all payload data has been received.
        if (!self->packet_handler_.ReadBody(size))
          self->ReadPacketBody();
        else {
          // All payload data has been received.
          // Push packet to incoming queue and start reading the next header.
          self->server_->PushPacket(std::move(self->packet_handler_.ExtractPacket()), self->id_);
          self->ReadPacketHead();
        }
      }
    );
  }

  void Session::Send(std::shared_ptr<std::vector<uint8_t>> buf)
  {
    spdlog::info("Session::Send");
    auto self = shared_from_this();
    socket_->async_write(
      buf->data(), 
      buf->size(),
      [self, buf](const beast::error_code& ec, std::size_t size)
      {
        // An error occured.
        if (ec)
          return spdlog::error("write: {}", ec.what());
        spdlog::info("write {} bytes to client", size);
      }
    );
  }
}
