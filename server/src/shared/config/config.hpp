#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lit {
// Server runtime settings (deployment concern, not part of the wire protocol).
struct NetConfig {
    std::string ip;
    std::uint16_t port;
    std::uint8_t io_threads;
};

// A playable faction (colour). Server-defined; sent to clients in Welcome.
struct FactionConfig {
    std::uint32_t id;  // 1..255; 0 = neutral / not chosen
    std::string name;
    std::uint32_t color;  // 0xRRGGBB
};

// Mirrors game.v1.AbilityKind: what using the ability does.
enum class AbilityKind : std::uint8_t {
    Melee = 1,       // hit every enemy within `range` around the player
    Projectile = 2,  // launch a projectile toward the aim; it flies `range` units
    Block = 3,       // no damage from any attack for `duration_ticks`; its own cooldown
};

// An ability on the 1–5 bar. Server-defined; sent to clients as Welcome.abilities.
struct AbilityConfig {
    std::uint32_t id;  // >= 1; InputFrame.ability refers to it (0 = the first ability)
    AbilityKind kind;
    std::string name;
    std::uint32_t cooldown_ticks;     // shared cooldown after use (45 = 0.75 s at 60 Hz)
    std::uint32_t damage;             // hp removed per hit
    std::uint32_t range;              // units: melee radius / projectile flight distance
    std::uint32_t projectile_speed;   // projectile: units per second (0 for melee)
    std::uint32_t projectile_radius;  // projectile: units (0 for melee)
    std::uint32_t duration_ticks;     // block: how long it lasts (0 for attacks)
};

// Mirrors game.v1.GameConfig from protocol.proto — the authoritative game rules
// the server sends to clients in Welcome. Scalar fields are uint32 to match the
// protobuf message; factions and abilities are sent as Welcome.factions /
// Welcome.abilities. Position units: 100 units = 1 cell (UNITS_PER_CELL).
struct GameConfig {
    std::uint32_t tick_rate;               // simulation ticks per second (60)
    std::uint32_t snapshot_rate;           // snapshots per second (20)
    std::uint32_t map_width;               // cells (100)
    std::uint32_t map_height;              // cells (100)
    std::uint32_t move_speed;              // units per second (300 = 3 cells/s)
    std::uint32_t max_hp;                  // (100)
    std::uint32_t respawn_delay_ticks;     // (300 = 5 s at 60 Hz)
    std::uint32_t reconnect_grace_ms;      // how long a dropped session is kept (30000)
    std::uint32_t capture_ticks;           // ticks for one player to capture a cell (60 = 1 s)
    std::uint32_t player_radius;           // body hit radius for projectiles, units (16)
    std::vector<FactionConfig> factions;   // selectable factions (colours)
    std::vector<AbilityConfig> abilities;  // bar order: abilities[0] is key 1 and the default
};

// Parses the "game" section of a config document. Throws on a missing or invalid
// value (unknown ability kind, no abilities, bad ids, a projectile that can't fly,
// a block without a duration).
GameConfig parse_game_config(const std::string& config_json);

class Config {
  public:
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    ~Config() = default;

    static const Config& get_instance(const std::string& filename) {
        static Config instance(filename);
        return instance;
    }

    const NetConfig& net_config() const noexcept { return net_config_; }

    const GameConfig& game_config() const noexcept { return game_config_; }

  private:
    explicit Config(const std::string& filename);
    void init_net_config(const std::string& filename);
    void init_game_config(const std::string& filename);

    NetConfig net_config_{};
    GameConfig game_config_{};
};
}  // namespace lit
