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
    std::ifstream file(filename);
    nlohmann::json config_data = nlohmann::json::parse(file);

    // Network.
    ip_ = config_data["server"]["ip"];
    port_ = config_data["server"]["port"];
    io_threads_ = config_data["server"]["io_threads"];
  
    spdlog::info("The server start at ip: {}", ip_);
    spdlog::info("The server start at port: {}", port_);
    spdlog::info("The server uses: {} I/O threads", io_threads_);

    // Game.
    tick_rate_ = config_data["game"]["tick_rate"];
    std::string map = config_data["game"]["map"];
    spdlog::info("map: {}", map);

    std::ifstream map_file(std::filesystem::current_path() / "config" / map);
    nlohmann::json map_data = nlohmann::json::parse(map_file);
    grid_x_ = map_data["grid_x"];
    grid_y_ = map_data["grid_y"];
    tile_ = map_data["tile"];
    player_start_x_ = map_data["player_start_x"];
    player_start_y_ = map_data["player_start_y"];
    player_offset_ = map_data["player_offset"];

    // Copy map.
    map_.resize(grid_x_ * grid_y_);
    std::vector<int> tmp = map_data["map"].get<std::vector<int>>();
    for(std::size_t i = 0; i < grid_x_ * grid_y_; i++)
      map_[i] = tmp[i];

    spdlog::info("The game tick rate: {} Hz", tick_rate_);
    spdlog::info("The game grid x count: {}", grid_x_);
    spdlog::info("The game grid y count: {}", grid_y_);
    spdlog::info("The game tile: {}", tile_);
    spdlog::info("Player start x: {}", player_start_x_);
    spdlog::info("Player start y: {}", player_start_y_);
    spdlog::info("Player offset: {}", player_offset_);

    // Debug map.
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 25; x++) {
            std::cout << (int)map_[y * 25+ x] << " ";
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
