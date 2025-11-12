#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/ssl/context.hpp>
#include <spdlog/spdlog.h>

#include "server/server.hpp"

int main(int argc, char* argv[])
{
  // Clear project namespaces for readability
  using namespace ep::net;

  // Check command line arguments.
  if (argc != 4) {
    spdlog::error("Usage: {} <address> <port> <threads>", argv[0]);
    return EXIT_FAILURE;
  }
  const auto address = net::ip::make_address(argv[1]);
  const auto port = static_cast<unsigned short>(std::atoi(argv[2]));
  const auto threads = std::max<int>(1, std::atoi(argv[3]));

  // The io_context is required for all I/O
  net::io_context ioc{threads};

  // The SSL context is required, and holds certificates
  ssl::context ctx(ssl::context::tlsv12_server);
  ctx.set_options(
    ssl::context::default_workarounds |
    ssl::context::no_sslv2 |
    ssl::context::no_sslv3 |
    ssl::context::single_dh_use);
  ctx.use_certificate_chain_file("certs/server.crt");
  ctx.use_private_key_file("certs/server.key", ssl::context::pem);
  ctx.set_verify_mode(ssl::verify_none);

  // Create and launch a listening port
  auto server = std::make_shared<Server>(ioc, ctx, tcp::endpoint{address, port});
  server->Run();

  // Run the I/O service on the requested number of threads
  std::vector<std::thread> v;
  v.reserve(threads - 1);
  for(auto i = threads - 1; i > 0; --i)
    v.emplace_back(
    [&ioc]
    {
        ioc.run();
    });
  ioc.run();

  return EXIT_SUCCESS;
}
