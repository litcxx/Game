#include <spdlog/spdlog.h>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ssl/context.hpp>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include "config/config.hpp"
#include "server/server.hpp"
#include "spdlog/common.h"
#include "subsystems/game_subsystem.hpp"
#include "subsystems/network_subsystem.hpp"
#include "world/world.hpp"

int main(int argc, char* argv[]) {
    // Clear project namespaces for readability.
    using namespace lit::net;
    using namespace lit::game;

    // Check command line arguments.
    if (argc != 2) {
        spdlog::error("Usage: {} <config>", argv[0]);
        return EXIT_FAILURE;
    }

    // Set spdlog level
    spdlog::set_level(spdlog::level::debug);

    // Initialize config.
    auto config = lit::Config::get_instance(argv[1]);

    // Initialize subsystems
    auto net_subsystem = std::make_shared<lit::NetworkSubsystem>();
    auto game_subsystem = std::make_shared<lit::GameSubsystem>();

    // Initialize the world
    auto world = std::make_shared<World>(net_subsystem, game_subsystem, config->game_config);
    const auto io_threads = std::max<int>(1, config->net_config.io_threads);
    const auto net_threads = std::max<int>(1, config->net_config.net_threads);

    // The io_context is required for all I/O.
    net::io_context ioc{io_threads};

    // The SSL context is required, and holds certificates.
    ssl::context ctx(ssl::context::tlsv12_server);
    ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
                    ssl::context::no_sslv3 | ssl::context::single_dh_use);
    ctx.use_certificate_chain_file("certs/server.crt");
    ctx.use_private_key_file("certs/server.key", ssl::context::pem);
    ctx.set_verify_mode(ssl::verify_none);

    // Start server
    auto server = std::make_shared<Server>(ioc, ctx, net_subsystem, game_subsystem);

    const auto address = net::ip::make_address(config->net_config.ip);
    const auto port = config->net_config.port;
    if (!server->start_listen(Tcp::endpoint{address, port})) {
        return EXIT_FAILURE;
    }

    // Run server
    server->run();

    // Run game
    std::jthread game_thread([&world] { world->game_loop(); });

    // Server send packets to clients
    std::vector<std::jthread> net_thread_pool;
    for (auto i = 0; i < net_threads; i++) {
        net_thread_pool.emplace_back([server] { server->sender(); });
    }

    // Run the I/O service on the requested number of threads.
    std::vector<std::jthread> io_thread_pool;
    for (auto i = 0; i < io_threads - 1; i++) {
        io_thread_pool.emplace_back([&ioc] { ioc.run(); });
    }
    ioc.run();

    return EXIT_SUCCESS;
}
