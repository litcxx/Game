#include "config.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

namespace lit {
Config::Config(std::string filename) {
    init_net_config(filename);
    init_game_config(filename);
    init_accounts_db_config(filename);
}

void Config::init_net_config(const std::string& filename) {
    std::ifstream config(filename);
    nlohmann::json config_data = nlohmann::json::parse(config);

    net_config.ip = config_data["server"]["ip"];
    net_config.port = config_data["server"]["port"];
    net_config.io_threads = config_data["server"]["io_threads"];
    net_config.net_threads = config_data["server"]["net_threads"];

    spdlog::info("The server start at ip: {}", net_config.ip);
    spdlog::info("The server start at port: {}", net_config.port);
    spdlog::info("The server uses: {} I/O threads", net_config.io_threads);
    spdlog::info("The server uses: {} net threads", net_config.net_threads);
}

void Config::init_game_config(const std::string& filename) {
    std::ifstream config(filename);
    nlohmann::json config_data = nlohmann::json::parse(config);

    game_config.tick_rate = config_data["game"]["tick_rate"];
    game_config.game_threads = config_data["game"]["game_threads"];

    // Map
    std::string map = config_data["game"]["map"];
    std::ifstream map_file(std::filesystem::current_path() / "config" / map);
    nlohmann::json map_data = nlohmann::json::parse(map_file);

    game_config.grid_x = map_data["grid_x"];
    game_config.grid_y = map_data["grid_y"];
    game_config.tile = map_data["tile"];

    // Copy map.
    game_config.map.resize(game_config.grid_x * game_config.grid_y);
    std::vector<int> tmp = map_data["map"].get<std::vector<int>>();
    for (std::size_t i = 0; i < game_config.grid_x * game_config.grid_y; i++)
        game_config.map[i] = static_cast<std::uint8_t>(tmp[i]);

    // Player
    std::string player = config_data["game"]["player"];
    std::ifstream player_file(std::filesystem::current_path() / "config" / player);
    nlohmann::json player_data = nlohmann::json::parse(player_file);

    game_config.player.width = player_data["type"]["default"]["width"];
    game_config.player.height = player_data["type"]["default"]["height"];
    game_config.player.player_start_x = player_data["type"]["default"]["player_start_x"];
    game_config.player.player_start_y = player_data["type"]["default"]["player_start_y"];
    game_config.player.player_offset = player_data["type"]["default"]["player_offset"];

    spdlog::info("Game tick rate: {} Hz", game_config.tick_rate);
    spdlog::info("Game threads: {}", game_config.game_threads);
    spdlog::info("Game grid x count: {}", game_config.grid_x);
    spdlog::info("Game grid y count: {}", game_config.grid_y);
    spdlog::info("Game tile: {}", game_config.tile);
    spdlog::info("Player width: {}", game_config.player.width);
    spdlog::info("Player height: {}", game_config.player.height);
    spdlog::info("Player start x: {}", game_config.player.player_start_x);
    spdlog::info("Player start y: {}", game_config.player.player_start_y);
    spdlog::info("Player offset: {}", game_config.player.player_offset);

    // Debug map.
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 25; x++) {
            std::cout << static_cast<int>(game_config.map[static_cast<std::size_t>(y * 25 + x)])
                      << " ";
        }
        std::cout << "\n";
    }
}

void Config::init_accounts_db_config(const std::string& filename) {
    std::ifstream config(filename);
    nlohmann::json config_data = nlohmann::json::parse(config);

    // TODO change to env-variables
    accounts_db_config.db_name = config_data["db"]["db_name"];
    accounts_db_config.host = config_data["db"]["host"];
    accounts_db_config.user = config_data["db"]["user"];
    accounts_db_config.password = config_data["db"]["password"];
    accounts_db_config.table_name = config_data["db"]["tables"]["accounts_table"];

    spdlog::info("User data base name: {}", accounts_db_config.db_name);
    spdlog::info("Data base host: {}", accounts_db_config.host);
    spdlog::info("Data base user: {}", accounts_db_config.user);
    spdlog::info("Data base passwrod: {}", accounts_db_config.password);
    spdlog::info("Data base table name: {}", accounts_db_config.table_name);
}

std::shared_ptr<Config> Config::get_instance(std::string filename) {
    static std::shared_ptr<Config> config(new Config(filename));
    return config;
}
}  // namespace lit
