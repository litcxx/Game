#include "server.hpp"

#include <boost/asio/ssl/context.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/strand.hpp>
#include <spdlog/spdlog.h>

#include "session/session.hpp"
#include "utils/asio_aliases.hpp"

Server::Server(boost::asio::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint) :
  ioc_(ioc),
  ctx_(ctx),
  acceptor_(ioc)
{
  boost::system::error_code ec;

  // Open the acceptor
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    spdlog::error("open: {}", ec.what());
    return;
  }

  // Allow address reuse
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    spdlog::error("set_option: {}", ec.what());
    return;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    spdlog::error("bind: {}", ec.what());
    return;
  }

  // Start listening for connections
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
        auto session = std::make_shared<Session>(std::move(socket), ctx_);
        session->Run();
      } else
        spdlog::error("async_accept: {}", ec.what());
      Run();
    }
  );
}
