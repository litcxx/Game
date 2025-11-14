#include "server.hpp"

#include <algorithm>

#include <boost/asio/ssl/context.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/strand.hpp>
#include <spdlog/spdlog.h>

#include "protocol/base_packet.hpp"
#include "session/session.hpp"
#include "socket/wss_socket.hpp"
#include "utils/asio_aliases.hpp"

namespace ep::net
{
  Server::Server(boost::asio::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint) :
    ioc_{ioc},
    ctx_{ctx},
    acceptor_{ioc},
    new_session_id_{0}
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
    new_session_id_++;
  }
  
  void Server::PushPacket(PacketData packet)
  {
    incoming_queue_.Push(std::move(packet));
  }

  void Server::CloseSession(std::shared_ptr<Session> session)
  {
    std::lock_guard lock(sessions_mutex_);
    auto it = std::find(sessions_.begin(), sessions_.end(), session);
    if (it != sessions_.end())
      sessions_.erase(it);
    else
      spdlog::error("can't find session in sessions buffer");
  }
}
