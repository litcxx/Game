#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "utils/asio_aliases.hpp"

class Server : public std::enable_shared_from_this<Server> {
public:
  explicit Server(boost::asio::io_context& ioc, tcp::endpoint endpoint);

  void Run();
private:

  net::io_context& ioc_;
  tcp::acceptor acceptor_;
};

#endif
