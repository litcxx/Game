#include "session.hpp"

#include <atomic>
#include <boost/asio/ssl/error.hpp>
#include <sys/types.h>

#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/error.hpp>
#include <spdlog/spdlog.h>

#include "protocol/server_packet.hpp"
#include "server/server.hpp"
#include "aliases/beast_aliases.hpp"

namespace ep::net
{
  Session::Session(std::shared_ptr<Server> server, std::shared_ptr<ISocket> socket, std::size_t id) :
    server_(server),
    socket_(socket),
    id_(id),
    state_(State::Connecting),
    sending_(ATOMIC_FLAG_INIT)
  {}

  void Session::Run()
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
            spdlog::warn("WebSocket was closed cleanly");
          else
            spdlog::error("Accept error: {}", ec.what());

          self->SetDisconnecting();
        } else {
          // Finally connected
          self->SetConnected();
        }

        self->ProcessState();

      }
    );
  }

  void Session::ProcessState()
  {
    switch (GetState()) {
      case State::Connecting:
        break;
      case State::Connected:
        // Add client to server and game
        server_->AddSession(shared_from_this());

        // Start reading client inputs
        ReadPacketHead();
        break;
      case State::Disconnecting:
        socket_->close();
        server_->CloseSession(id_);
        SetDisconnected();
        break;
      case State::Disconnected:
        break;
      case State::User:
        break;
      default:
        spdlog::error("Unknown session state");
    }
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
            spdlog::warn("WebSocket was closed cleanly");
          else
            spdlog::error("Read header error: {}", ec.what());

          // Close session process
          self->SetDisconnecting();
          self->ProcessState();
          return;
        }

        // Continue reading the header, untill the complete PacketHead is recived.
        if (!self->packet_handler_.UpdateHeadSize(size))
          return self->ReadPacketHead();

        // Check if packet contains payload data, then read the data
        if (self->packet_handler_.BodySizeLeft())
          return self->ReadPacketBody();

        // Packet does not contain payload data, push to handler
        auto packet = std::make_unique<ServerPacket>(self->packet_handler_.ExtractPacket(), self->id_);
        self->server_->PushPacket(std::move(packet));

        // Continue reading next packet
        self->ReadPacketHead();
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
            spdlog::warn("WebSocket was closed cleanly");
          else
            spdlog::error("Read body error: {}", ec.what());

          // Close session process
          self->SetDisconnecting();
          self->ProcessState();
          return;
        }

        // Continue reading payload data untill all data has been received.
        if (!self->packet_handler_.UpdateBodySize(size))
          return self->ReadPacketBody();

        // All payload data has been received, push to handler
        auto packet = std::make_unique<ServerPacket>(self->packet_handler_.ExtractPacket(), self->id_);
        self->server_->PushPacket(std::move(packet));

        // Continue reading next packet
        self->ReadPacketHead();
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
    // Check if session is disconnected
    if (!IsConnected())
      return spdlog::info("Return from send operation, client is disconneted");

    // Check if send queue is empty
    if (out_queue_.Empty())
      return spdlog::info("Return from send, queue is empty");

    // Check if previous sending finished
    if (StartSending())
      return spdlog::info("Return from send operation, previous send is not finished");

    auto buf = out_queue_.TryPop();
    // This check should never pass
    if (!buf)
      return spdlog::error("buffer is nullopt:\nfile: {} line: {}", __FILE__, __LINE__);

    auto self = shared_from_this();
    socket_->async_write(
      (*buf)->data(), 
      (*buf)->size(),
      [self, buf](const beast::error_code& ec, std::size_t size)
      {
        // an error occured
        if (ec) {
          // client close connection
          if (ec == websocket::error::closed)
            spdlog::warn("WebSocket was closed cleanly");
          else
            spdlog::error("Write error: {}", ec.what());

          // Close session process
          self->SetDisconnecting();
          self->ProcessState();
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
