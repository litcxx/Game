#include <boost/asio/ip/address.hpp>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/ssl/context.hpp>
#include <spdlog/spdlog.h>
#include "spdlog/common.h"

#include "server/server.hpp"
#include "world/world.hpp"
#include "subsystems/network_subsystem.hpp"
#include "subsystems/game_subsystem.hpp"
#include "config/config.hpp"
#include "db/sql_connection.hpp"

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
  
  // Set spdlog level
  spdlog::set_level(spdlog::level::debug);

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
  const auto io_threads = std::max<int>(1, config->net_config_.io_threads_);
  const auto net_threads = std::max<int>(1, config->net_config_.net_threads_);

  // The io_context is required for all I/O.
  net::io_context ioc{io_threads};

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

  // Server send packets to clients
  std::vector<std::jthread> net_thread_pool;
  for(auto i = 0; i < net_threads; i++) {
    net_thread_pool.emplace_back(
      [server]
      { 
        server->Sender(); 
      }
    );
  }

  // Run the I/O service on the requested number of threads.
  std::vector<std::jthread> io_thread_pool;
  for(auto i = 0; i < io_threads - 1; i++) {
    io_thread_pool.emplace_back(
      [&ioc]
      {
          ioc.run();
      }
    );
  }
  ioc.run();

  return EXIT_SUCCESS;
}
