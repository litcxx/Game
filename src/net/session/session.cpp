#include "session.hpp"

#include <atomic>
#include <cstdint>
#include <sys/types.h>

#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/error.hpp>
#include <spdlog/spdlog.h>

#include "server/server.hpp"
#include "aliases/asio_aliases.hpp"
#include "aliases/beast_aliases.hpp"

namespace ep::net
{
  Session::Session(std::shared_ptr<Server> server, std::shared_ptr<ISocket> socket, std::size_t id) :
    server_(server),
    socket_(socket),
    id_(id),
    state_(ATOMIC_FLAG_INIT),
    sending_(ATOMIC_FLAG_INIT)
  {}

  void Session::Run()
  {
    spdlog::info("Session::Run");
    auto self = shared_from_this();
    socket_->async_handshake(
      [self](const beast::error_code& ec)
      {
        // an error occured
        if (ec) {
          // client close connection
          if (ec == websocket::error::closed)
            return spdlog::info("session was closed");
          else
            return spdlog::warn("async_handshake: {}", ec.what());

          self->socket_->close();
          return;
        }

        self->Accept();
      }
    );
  }

  void Session::Accept()
  {

    spdlog::info("Session::Accept");
    // TODO set timeout
    // TODO set decorator

    auto self = shared_from_this();
    socket_->async_accept(
      [self](const beast::error_code &ec)
      {
        // an error occured
        if (ec) {
          // client close connection
          if (ec == websocket::error::closed)
            return spdlog::info("session was closed");
          else
            return spdlog::warn("accept: {}", ec.what());

          self->socket_->close();
          return;
        }

        // Finally connected
        self->SetConnected();

        // Add client to server and game
        self->server_->AddSession(self);

        // Start reading client inputs
        self->ReadPacketHead();
      }
    );
  }

  void Session::ReadPacketHead()
  {
    spdlog::info("Session::ReadPacketHead");
    auto self = shared_from_this();
    socket_->async_read_some(
      packet_handler_.HeadCurrentData(),
      packet_handler_.HeadSizeLeft(),
      [self](const beast::error_code& ec, std::size_t size)
      {
        // an error occured
        if (ec) {
          // client close connection
          if (ec == websocket::error::closed)
            spdlog::warn("session was closed");
          else
            spdlog::error("read : {}", ec.what());

          // Close session process
          self->SetDisconneted();
          self->socket_->close();
          self->server_->CloseSession(self->id_);
          return;
        }

        // Continue reading the header, untill the complete PacketHead is recived.
        if (!self->packet_handler_.UpdateHeadSize(size))
          return self->ReadPacketHead();
        else {
          // All header has been received. If payload size == 0 push to queue 
          // and continue read next header. Otherwise read the body.
          if (self->packet_handler_.BodySizeLeft())
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
      packet_handler_.BodyCurrentData(),
      packet_handler_.BodySizeLeft(),
      [self](const beast::error_code& ec, std::size_t size)
      {
        // an error occured
        if (ec) {
          // client close connection
          if (ec == websocket::error::closed)
            spdlog::warn("session was closed");
          else
            spdlog::error("read : {}", ec.what());

          // Close session process
          self->SetDisconneted();
          self->socket_->close();
          self->server_->CloseSession(self->id_);
          return;
        }

        // Continue reading payload data untill all data has been received.
        if (!self->packet_handler_.UpdateBodySize(size))
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

  void Session::PushToSend(SendBuffer packet)
  {
    out_queue_.Push(packet); 
    Send();
  }

  void Session::Send()
  {
    // spdlog::info("Session::Send");
    if (!IsConnected())
      return spdlog::warn("Close send operation, client is disconneted");

    if (out_queue_.Empty())
      return spdlog::warn("Return from send, send queue is empty");

    if (StartSending())
      return spdlog::warn("Close send operation, previous send is not finished");

    auto buf = out_queue_.TryPop();

    auto self = shared_from_this();
    socket_->async_write(
      buf->data(), 
      buf->size(),
      [self, buf](const beast::error_code& ec, std::size_t size)
      {
        // an error occured
        if (ec) {
          // client close connection
          if (ec == websocket::error::closed)
            spdlog::warn("session was closed");
          else
            spdlog::error("read : {}", ec.what());

          // Close session process
          self->SetDisconneted();
          self->socket_->close();
          self->server_->CloseSession(self->id_);
          return;
        }
        // spdlog::info("write {} bytes to client", size);

        self->sending_.clear();
        // If out queue is not empty send again
        if (!self->out_queue_.Empty())
          self->Send();
      }
    );
  }
}
