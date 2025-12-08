#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace ep
{
  struct NetConfig {
    std::string ip_;
    std::uint16_t port_;
    std::uint16_t io_threads_;
  };

  struct PlayerConfig {
    std::uint8_t width_;
    std::uint8_t height_;
    std::uint16_t player_start_x_;
    std::uint16_t player_start_y_;
    std::uint16_t player_offset_;
  };

  struct GameConfig {
    // Game
    std::uint8_t tick_rate_;

    // Map
    std::uint8_t tile_;
    std::uint16_t grid_x_;
    std::uint16_t grid_y_;
    std::vector<std::uint8_t> map_;

    // Player
    PlayerConfig player_;
  };

  class Config {
  public:
    // Filename is only used on the first call.
    static std::shared_ptr<Config> GetInstance(std::string filename = "null");
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    ~Config() = default;

    NetConfig net_config_;
    GameConfig game_config_;

  private:
    explicit Config(std::string filename);
    void InitNetConfig(const std::string& filename);
    void InitGameConfig(const std::string& filename);
  };
}
