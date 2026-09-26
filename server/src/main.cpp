#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <cstdlib>
#include <stop_token>
#include <thread>
#include <vector>

#include "config/config.hpp"
#include "net/client_event.hpp"
#include "server/server.hpp"
#include "spdlog/common.h"
#include "world/world.hpp"

namespace asio = boost::asio;

// Fully qualified (no `using namespace lit`): unqualified `game` would be
// ambiguous between lit::game and the protobuf ::game namespace.
int main(int argc, char* argv[]) {
    if (argc != 2) {
        spdlog::error("Usage: {} <config>", argv[0]);
        return EXIT_FAILURE;
    }

    spdlog::set_level(spdlog::level::debug);

    const lit::Config& config = lit::Config::get_instance(argv[1]);
    const auto io_threads = std::max<int>(1, config.net_config().io_threads);

    // BOOST
    asio::io_context io(io_threads);
    asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&io](auto, auto) { io.stop(); });

    // Ordered net→game events (messages + disconnects), produced by the network,
    // consumed by the game loop.
    lit::TSQueue<lit::ClientEvent> incoming_events;

    // --- I/O SERVER (also the game loop's gateway back to clients) ---
    lit::net::Server server(io, config.net_config(), incoming_events);

    // --- GAME LOOP ---
    lit::game::World world(incoming_events, server, config.game_config());
    std::jthread game_thread([&world](std::stop_token stop) { world.run(stop); });

    asio::co_spawn(io, server.do_listen(), asio::detached);

    // Run the I/O service on the configured number of threads.
    std::vector<std::jthread> io_thread_pool;
    for (auto i = 0; i < io_threads - 1; i++) {
        io_thread_pool.emplace_back([&io] { io.run(); });
    }
    io.run();

    return EXIT_SUCCESS;
}
