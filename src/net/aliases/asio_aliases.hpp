#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ep::net {
namespace net = boost::asio;
namespace ssl = net::ssl;
using Tcp = net::ip::tcp;
}  // namespace ep::net
