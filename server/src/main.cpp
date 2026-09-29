#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <chrono>
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

    // The config logs itself at the default level (info); its own level applies after.
    const lit::Config& config = lit::Config::get_instance(argv[1]);
    spdlog::set_level(spdlog::level::from_str(config.log_config().level));
    const auto io_threads = std::max<int>(1, config.net_config().io_threads);

    // BOOST
    asio::io_context io(io_threads);
    asio::signal_set signals(io, SIGINT, SIGTERM);

    // Ordered net→game events (messages + disconnects), produced by the network,
    // consumed by the game loop.
    lit::TSQueue<lit::ClientEvent> incoming_events;

    // --- I/O SERVER (also the game loop's gateway back to clients) ---
    lit::net::Server server(io, config.net_config(), incoming_events);

    // --- GAME LOOP ---
    lit::game::World world(incoming_events, server, config.game_config(),
                           std::chrono::seconds{config.log_config().metrics_interval_s});
    std::jthread game_thread([&world](std::stop_token stop) { world.run(stop); });

    // Graceful shutdown: stop accepting and close sessions so their coroutines
    // finish; io.run() then drains on its own. (io.stop() would abandon in-flight
    // session coroutines and crash during teardown.)
    signals.async_wait([&server, &game_thread](auto, auto) {
        spdlog::info("Shutdown signal received");
        game_thread.request_stop();  // stop the game loop first so it stops queuing
                                     // snapshots into sessions...
        server.stop();               // ...then close sessions; io.run() drains cleanly.
    });

    server.listen();

    // Run the I/O service on the configured number of threads.
    std::vector<std::jthread> io_thread_pool;
    for (auto i = 0; i < io_threads - 1; i++) {
        io_thread_pool.emplace_back([&io] { io.run(); });
    }
    io.run();

    return EXIT_SUCCESS;
}
