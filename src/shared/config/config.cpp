#include "config.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <vector>

namespace ep
{
  Config::Config(std::string filename)
  {
    InitNetConfig(filename);
    InitGameConfig(filename);
  }

  void Config::InitNetConfig(const std::string& filename)
  {
    std::ifstream config(filename);
    nlohmann::json config_data = nlohmann::json::parse(config);

    net_config_.ip_ = config_data["server"]["ip"];
    net_config_.port_ = config_data["server"]["port"];
    net_config_.io_threads_ = config_data["server"]["io_threads"];
  
    spdlog::info("The server start at ip: {}", net_config_.ip_);
    spdlog::info("The server start at port: {}", net_config_.port_);
    spdlog::info("The server uses: {} I/O threads", net_config_.io_threads_);
  }

  void Config::InitGameConfig(const std::string& filename)
  {
    std::ifstream config(filename);
    nlohmann::json config_data = nlohmann::json::parse(config);

    game_config_.tick_rate_ = config_data["game"]["tick_rate"];

    // Map
    std::string map = config_data["game"]["map"];
    std::ifstream map_file(std::filesystem::current_path() / "config" / map);
    nlohmann::json map_data = nlohmann::json::parse(map_file);

    game_config_.grid_x_ = map_data["grid_x"];
    game_config_.grid_y_ = map_data["grid_y"];
    game_config_.tile_ = map_data["tile"];

    // Copy map.
    game_config_.map_.resize(game_config_.grid_x_ * game_config_.grid_y_);
    std::vector<int> tmp = map_data["map"].get<std::vector<int>>();
    for(std::size_t i = 0; i < game_config_.grid_x_ * game_config_.grid_y_; i++)
      game_config_.map_[i] = tmp[i];

    // Player
    std::string player = config_data["game"]["player"];
    std::ifstream player_file(std::filesystem::current_path() / "config" / player);
    nlohmann::json player_data = nlohmann::json::parse(player_file);

    game_config_.player_.width_ = player_data["type"]["default"]["width"];
    game_config_.player_.height_ = player_data["type"]["default"]["height"];
    game_config_.player_.player_start_x_ = player_data["type"]["default"]["player_start_x"];
    game_config_.player_.player_start_y_ = player_data["type"]["default"]["player_start_y"];
    game_config_.player_.player_offset_ = player_data["type"]["default"]["player_offset"];

    spdlog::info("The game tick rate: {} Hz", game_config_.tick_rate_);
    spdlog::info("The game grid x count: {}", game_config_.grid_x_);
    spdlog::info("The game grid y count: {}", game_config_.grid_y_);
    spdlog::info("The game tile: {}", game_config_.tile_);
    spdlog::info("Player width: {}", game_config_.player_.width_);
    spdlog::info("Player height: {}", game_config_.player_.height_);
    spdlog::info("Player start x: {}", game_config_.player_.player_start_x_);
    spdlog::info("Player start y: {}", game_config_.player_.player_start_y_);
    spdlog::info("Player offset: {}", game_config_.player_.player_offset_);

    // Debug map.
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 25; x++) {
            std::cout << (int)game_config_.map_[y * 25+ x] << " ";
        }
        std::cout << "\n";
    }
  }

  std::shared_ptr<Config> Config::GetInstance(std::string filename)
  {
    static std::shared_ptr<Config> config(new Config(filename));
    return config;
  }
}
