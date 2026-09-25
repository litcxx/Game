#include "config.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <nlohmann/json.hpp>

namespace lit {
Config::Config(const std::string& filename) {
    init_net_config(filename);
    init_game_config(filename);
}

void Config::init_net_config(const std::string& filename) {
    std::ifstream config(filename);
    nlohmann::json config_data = nlohmann::json::parse(config);

    // .at() throws a clear "key not found" instead of silently yielding null.
    const auto& server = config_data.at("server");
    net_config_.ip = server.at("ip");
    net_config_.port = server.at("port");
    net_config_.io_threads = server.at("io_threads");

    spdlog::info("Server ip={} port={} io_threads={}", net_config_.ip, net_config_.port,
                 net_config_.io_threads);
}

void Config::init_game_config(const std::string& filename) {
    std::ifstream config(filename);
    nlohmann::json config_data = nlohmann::json::parse(config);

    // Field names and values match game.v1.GameConfig (protocol.proto).
    const auto& game = config_data.at("game");
    game_config_.tick_rate = game.at("tick_rate");
    game_config_.snapshot_rate = game.at("snapshot_rate");
    game_config_.map_width = game.at("map_width");
    game_config_.map_height = game.at("map_height");
    game_config_.move_speed = game.at("move_speed");
    game_config_.max_hp = game.at("max_hp");
    game_config_.attack_range = game.at("attack_range");
    game_config_.attack_cooldown_ticks = game.at("attack_cooldown_ticks");
    game_config_.respawn_delay_ticks = game.at("respawn_delay_ticks");
    game_config_.reconnect_grace_ms = game.at("reconnect_grace_ms");

    spdlog::info("Game tick_rate={} snapshot_rate={} map={}x{}", game_config_.tick_rate,
                 game_config_.snapshot_rate, game_config_.map_width, game_config_.map_height);
    spdlog::info("Game move_speed={} max_hp={} attack_range={} attack_cooldown_ticks={}",
                 game_config_.move_speed, game_config_.max_hp, game_config_.attack_range,
                 game_config_.attack_cooldown_ticks);
    spdlog::info("Game respawn_delay_ticks={} reconnect_grace_ms={}",
                 game_config_.respawn_delay_ticks, game_config_.reconnect_grace_ms);
}
}  // namespace lit
