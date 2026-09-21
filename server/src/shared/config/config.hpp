#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace lit {
struct NetConfig {
    std::string ip;
    std::uint16_t port;
    std::uint8_t io_threads;
    std::uint8_t net_threads;
};

struct PlayerConfig {
    std::uint8_t width;
    std::uint8_t height;
    std::uint16_t player_start_x;
    std::uint16_t player_start_y;
    std::uint16_t player_offset;
};

struct AccountsDBConfig {
    std::string db_name;
    std::string host;
    std::string user;
    std::string password;
    std::string table_name;
};

struct GameConfig {
    // Game
    std::uint8_t tick_rate;
    std::uint8_t game_threads;

    // Map
    std::uint8_t tile;
    std::uint16_t grid_x;
    std::uint16_t grid_y;
    std::vector<std::uint8_t> map;

    // Player
    PlayerConfig player;
};

class Config {
  public:
    // Filename is only used on the first call.
    static std::shared_ptr<Config> get_instance(std::string filename = "null");
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    ~Config() = default;

    NetConfig net_config;
    GameConfig game_config;
    AccountsDBConfig accounts_db_config;

  private:
    explicit Config(std::string filename);
    void init_net_config(const std::string& filename);
    void init_game_config(const std::string& filename);
    void init_accounts_db_config(const std::string& filename);
};
}  // namespace lit
