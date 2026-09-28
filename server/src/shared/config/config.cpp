#include "config.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

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

namespace {
AbilityKind parse_ability_kind(const std::string& kind) {
    if (kind == "melee") return AbilityKind::Melee;
    if (kind == "projectile") return AbilityKind::Projectile;
    if (kind == "block") return AbilityKind::Block;
    throw std::runtime_error("config: unknown ability kind '" + kind + "'");
}

AbilityConfig parse_ability(const nlohmann::json& a) {
    AbilityConfig ability{};
    ability.id = a.at("id");
    ability.kind = parse_ability_kind(a.at("kind").get<std::string>());
    ability.name = a.at("name");
    ability.cooldown_ticks = a.at("cooldown_ticks");
    // Attacks must state their damage and reach; a block has neither.
    const bool attack = ability.kind != AbilityKind::Block;
    ability.damage = attack ? a.at("damage").get<std::uint32_t>() : a.value("damage", 0U);
    ability.range = attack ? a.at("range").get<std::uint32_t>() : a.value("range", 0U);
    ability.projectile_speed = a.value("projectile_speed", 0U);
    ability.projectile_radius = a.value("projectile_radius", 0U);
    ability.duration_ticks = a.value("duration_ticks", 0U);

    if (ability.id == 0) throw std::runtime_error("config: ability id must be >= 1");
    if (ability.kind == AbilityKind::Projectile &&
        (ability.projectile_speed == 0 || ability.range == 0)) {
        throw std::runtime_error("config: projectile ability '" + ability.name +
                                 "' needs projectile_speed > 0 and range > 0");
    }
    if (ability.kind == AbilityKind::Block && ability.duration_ticks == 0) {
        throw std::runtime_error("config: block ability '" + ability.name +
                                 "' needs duration_ticks > 0");
    }
    return ability;
}
}  // namespace

GameConfig parse_game_config(const std::string& config_json) {
    const nlohmann::json doc = nlohmann::json::parse(config_json);

    // .at() throws a clear "key not found" instead of silently yielding null.
    // Field names and values match game.v1.GameConfig (protocol.proto).
    const auto& game = doc.at("game");
    GameConfig config{};
    config.tick_rate = game.at("tick_rate");
    config.snapshot_rate = game.at("snapshot_rate");
    config.map_width = game.at("map_width");
    config.map_height = game.at("map_height");
    config.move_speed = game.at("move_speed");
    config.max_hp = game.at("max_hp");
    config.respawn_delay_ticks = game.at("respawn_delay_ticks");
    config.reconnect_grace_ms = game.at("reconnect_grace_ms");
    config.capture_ticks = game.at("capture_ticks");
    config.player_radius = game.at("player_radius");
    config.vision_radius = game.at("vision_radius");
    if (config.vision_radius == 0) {
        throw std::runtime_error("config: vision_radius must be > 0");
    }

    for (const auto& f : game.at("factions")) {
        config.factions.push_back(FactionConfig{f.at("id"), f.at("name"), f.at("color")});
    }
    for (const auto& a : game.at("abilities")) {
        AbilityConfig ability = parse_ability(a);
        for (const auto& other : config.abilities) {
            if (other.id == ability.id) {
                throw std::runtime_error("config: duplicate ability id " +
                                         std::to_string(ability.id));
            }
        }
        config.abilities.push_back(std::move(ability));
    }
    if (config.abilities.empty()) {
        throw std::runtime_error("config: at least one ability is required");
    }
    return config;
}

void Config::init_game_config(const std::string& filename) {
    std::ifstream file(filename);
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    game_config_ = parse_game_config(text);

    spdlog::info("Game tick_rate={} snapshot_rate={} map={}x{}", game_config_.tick_rate,
                 game_config_.snapshot_rate, game_config_.map_width, game_config_.map_height);
    spdlog::info("Game move_speed={} max_hp={} player_radius={} vision_radius={} abilities={}",
                 game_config_.move_speed, game_config_.max_hp, game_config_.player_radius,
                 game_config_.vision_radius, game_config_.abilities.size());
    spdlog::info("Game respawn_delay_ticks={} reconnect_grace_ms={} factions={}",
                 game_config_.respawn_delay_ticks, game_config_.reconnect_grace_ms,
                 game_config_.factions.size());
}
}  // namespace lit
