#include "config.hpp"

#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ep
{
  Config::Config(std::string filename)
  {
    std::ifstream file(filename);
    nlohmann::json j = nlohmann::json::parse(file);

    // Network.
    ip_ = j["server"]["ip"];
    port_ = j["server"]["port"];
    io_threads_ = j["server"]["io_threads"];
  
    spdlog::info("The server start at ip: {}", ip_);
    spdlog::info("The server start at port: {}", port_);
    spdlog::info("The server uses: {} I/O threads", io_threads_);

    // Game.
    tick_rate_ = j["game"]["tick_rate"];
    spdlog::info("The game tick rate: {} Hz", tick_rate_);
  }

  std::shared_ptr<Config> Config::GetInstance(std::string filename)
  {
    static std::shared_ptr<Config> config(new Config(filename));
    return config;
  }
}
