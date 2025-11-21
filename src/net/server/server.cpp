#include "server.hpp"

#include <algorithm>

#include <boost/asio/ssl/context.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/strand.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>

#include "protocol/base_packet.hpp"
#include "protocol/events.hpp"
#include "session/session.hpp"
#include "socket/wss_socket.hpp"

namespace ep::net
{
  Server::Server(boost::asio::io_context& ioc, 
                 ssl::context& ctx, 
                 tcp::endpoint endpoint, 
                 std::shared_ptr<NetworkSubsystem> net_susbsystem, 
                 std::shared_ptr<game::GameSubsystem> game_subsystem) :
    ioc_{ioc},
    ctx_{ctx},
    acceptor_{ioc},
    new_session_id_{0},
    net_susbsystem_(net_susbsystem),
    game_susbsystem_(game_subsystem)
{
    boost::system::error_code ec;

    // Open the acceptor.
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      spdlog::error("open: {}", ec.what());
      return;
    }

    // Allow address reuse.
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
      spdlog::error("set_option: {}", ec.what());
      return;
    }

    // Bind to the server address.
    acceptor_.bind(endpoint, ec);
    if (ec) {
      spdlog::error("bind: {}", ec.what());
      return;
    }

    // Start listening for connections.
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
      spdlog::error("listen: {}", ec.what());
      return;
    }
  }

  void Server::Run()
  {
    acceptor_.async_accept(
      net::make_strand(ioc_),
      [this](const boost::system::error_code& ec, tcp::socket socket)
      {
        if (!ec) {
          auto wsssocket = std::make_unique<WSSSocket>(std::move(socket), ctx_);
          std::size_t id = new_session_id_.fetch_add(1);
          auto session = std::make_shared<Session>(shared_from_this(), std::move(wsssocket), id);
          AddSession(session);
          session->Run();
        } else {
          spdlog::error("async_accept: {}", ec.what());
        }

        Run();
      }
    );
  }

  void Server::AddSession(std::shared_ptr<Session> session) noexcept
  {
    std::lock_guard lock(sessions_mutex_);
    sessions_.push_back(session);
    game_susbsystem_->event_queue_.Push(Event(ep::EventCode::AddNewPlayer, session->GetID()));
  }
 
  void Server::Broadcast()
  {
    spdlog::info("Server::Broadcast");
    for (;;) {
      auto packet = net_susbsystem_->out_queue_.WaitAndPop();
      spdlog::info("net_susbsystem_->out_queue_.WaitAndPop");

      // Make flat buffer from packet.
      auto buf = std::make_shared<std::vector<std::uint8_t>>(std::move(packet->MakeBuffer()));
      spdlog::info("buffer length: {}", buf->size());

      spdlog::info("send packet to all sessions");
      for (const auto& session : sessions_) {
        session->Send(buf);
      }
    }
  }

  void Server::PushPacket(NetPacket packet, std::size_t id)
  {
    packet.SetID(id);
    net_susbsystem_->in_queue_.Push(std::move(packet));
  }

  void Server::CloseSession(std::size_t id)
  {
    std::lock_guard lock(sessions_mutex_);
    auto it = std::find_if(sessions_.begin(), sessions_.end(), [id](std::shared_ptr<Session> sess) { return sess->GetID() == id; });
    if (it != sessions_.end())
      sessions_.erase(it);
    else
      spdlog::error("can't find session in sessions buffer");
    game_susbsystem_->event_queue_.Push(Event(ep::EventCode::RemovePlayer, id));
  }
}
