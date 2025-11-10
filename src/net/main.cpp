#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

#include "server/server.hpp"

int main(int argc, char* argv[])
{
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

  // Create and launch a listening port
  auto server = std::make_shared<Server>(ioc, tcp::endpoint{address, port});
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
