#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <thread>
#include <vector>

#include "config/config.hpp"
#include "net/client_event.hpp"
#include "persist/save_store.hpp"
#include "persist/save_writer.hpp"
#include "persist/world_save.hpp"
#include "save.pb.h"
#include "server/server.hpp"
#include "spdlog/common.h"
#include "world/world.hpp"

namespace asio = boost::asio;

// Fully qualified (no `using namespace lit`): unqualified `game` would be
// ambiguous between lit::game and the protobuf ::game namespace.
namespace {
// The newest save in `store`, if there is one (GAME-019). A save this server
// can't take is refused: no new world over it unless asked for (--fresh).
std::optional<lit::save::WorldSave> load_save(const lit::game::SaveStore& store) {
    const auto started = std::chrono::steady_clock::now();
    auto loaded = store.load_latest();
    if (!loaded) throw std::runtime_error(loaded.error());
    if (!*loaded) {
        spdlog::info("No world save in {}: a new world", store.dir().string());
        return std::nullopt;
    }
    lit::save::WorldSave save;
    if (!save.ParseFromString((*loaded)->payload)) {
        throw std::runtime_error((*loaded)->path.string() + ": not a world save (cannot parse)");
    }
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    spdlog::info("Loaded {} ({} bytes, {} characters) in {} ms", (*loaded)->path.string(),
                 (*loaded)->payload.size(), save.characters_size(), took.count());
    return save;
}
}  // namespace

int main(int argc, char* argv[]) {
    const bool fresh = argc == 3 && std::string_view{argv[2]} == "--fresh";
    if (argc != 2 && !fresh) {
        spdlog::error("Usage: {} <config> [--fresh]", argv[0]);
        spdlog::error("  --fresh: a new world, whatever is saved (the save files stay)");
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

    // --- WORLD SAVE (GAME-019) ---
    // Declared before the World: it outlives the game loop, so the snapshot the
    // loop takes as it stops is still written (its destructor waits for it).
    const lit::SaveConfig& save_config = config.save_config();
    lit::game::SaveStore store(save_config.dir, save_config.keep);
    std::optional<lit::save::WorldSave> saved;
    if (fresh) {
        spdlog::warn(
            "--fresh: a new world; the saves in {} stay, the next one is numbered after them",
            save_config.dir);
    } else {
        try {
            saved = load_save(store);
        } catch (const std::exception& e) {
            spdlog::critical("Cannot load the world: {}", e.what());
            spdlog::critical(
                "Fix or move that file away (an older save is then loaded), or start "
                "with --fresh for a new world");
            return EXIT_FAILURE;
        }
    }
    lit::game::SaveWriter save_writer(std::move(store));

    // --- GAME LOOP ---
    lit::game::World world(incoming_events, server, config.game_config(),
                           std::chrono::seconds{config.log_config().metrics_interval_s});
    if (saved) {
        const auto restored = world.restore(*saved);
        if (!restored) {
            spdlog::critical("Cannot load the world: {}", restored.error());
            spdlog::critical("Start with --fresh for a new world (the save files stay)");
            return EXIT_FAILURE;
        }
        if (*restored == lit::game::SeasonLoad::Continued) {
            spdlog::info("Season {}: the world goes on", config.game_config().season_id);
        } else {
            spdlog::warn(
                "Season {} is new: the characters stay, factions and territory start again",
                config.game_config().season_id);
        }
    }
    world.save_to(save_writer, std::chrono::seconds{save_config.interval_s});
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
