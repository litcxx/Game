#include <boost/asio/ip/address.hpp>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/ssl/context.hpp>
#include <spdlog/spdlog.h>

#include "server/server.hpp"
#include "world/world.hpp"
#include "subsystems/network_subsystem.hpp"
#include "subsystems/game_subsystem.hpp"
#include "config/config.hpp"

int main(int argc, char* argv[])
{
  // Clear project namespaces for readability.
  using namespace ep::net;
  using namespace ep::game;

  // Check command line arguments.
  if (argc != 2) {
    spdlog::error("Usage: {} <config>", argv[0]);
    return EXIT_FAILURE;
  }

  // Initialize config.
  auto config = ep::Config::GetInstance(argv[1]);

  // Initialize network subsystem.
  auto net_subsystem = std::make_shared<ep::NetworkSubsystem>();

  // Initialize game subsystem.
  auto game_subsystem = std::make_shared<ep::GameSubsystem>();

  // Initialize the world and run game loop.
  auto world = std::make_shared<World>(net_subsystem, 
                                       game_subsystem,
                                       config->game_config_);
  // Game main loop.
  std::jthread game_thread(
    [&world]
    {
      world->GameLoop();
    }
  );

  const auto address = net::ip::make_address(config->net_config_.ip_);
  const auto port = config->net_config_.port_;
  const auto threads = std::max<int>(1, config->net_config_.io_threads_);

  // The io_context is required for all I/O.
  net::io_context ioc{threads};

  // The SSL context is required, and holds certificates.
  ssl::context ctx(ssl::context::tlsv12_server);
  ctx.set_options(
    ssl::context::default_workarounds |
    ssl::context::no_sslv2 |
    ssl::context::no_sslv3 |
    ssl::context::single_dh_use);
  ctx.use_certificate_chain_file("certs/server.crt");
  ctx.use_private_key_file("certs/server.key", ssl::context::pem);
  ctx.set_verify_mode(ssl::verify_none);

  // Create and launch a listening port.
  auto server = std::make_shared<Server>(ioc, 
                                         ctx, 
                                         tcp::endpoint{address, port},
                                         net_subsystem,
                                         game_subsystem);
  // Server main loop.
  server->Run();

  // Server boradcast all general packets.
  std::jthread sender_tread(
    [server]
    { 
      server->Sender(); 
    }
  );

  // Run the I/O service on the requested number of threads.
  std::vector<std::jthread> v;
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
