#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace lit::net {
namespace net = boost::asio;
namespace ssl = net::ssl;
using Tcp = net::ip::tcp;
}  // namespace lit::net
