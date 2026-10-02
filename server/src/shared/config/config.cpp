#include "config.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace lit {
Config::Config(const std::string& filename) {
    init_net_config(filename);
    init_game_config(filename);
    init_log_config(filename);
    init_save_config(filename);
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
// Every limit is required and must be > 0.
LimitsConfig parse_limits(const nlohmann::json& l) {
    const auto positive = [&l](const char* key) {
        const std::uint32_t value = l.at(key);
        if (value == 0)
            throw std::runtime_error(std::string{"config: limits."} + key + " must be > 0");
        return value;
    };
    LimitsConfig limits;
    limits.max_input_frames = positive("max_input_frames");
    limits.input_queue = positive("input_queue");
    limits.messages_per_second = positive("messages_per_second");
    limits.handshake_timeout_ms = positive("handshake_timeout_ms");
    limits.idle_timeout_ms = positive("idle_timeout_ms");
    limits.resync_window_ms = positive("resync_window_ms");
    return limits;
}
}  // namespace

namespace {
// One capital per faction, on the map, the zones apart: centres more than the two
// radii apart, so no cell can belong to two zones.
void validate_capitals(const GameConfig& config) {
    const auto has_faction = [&config](std::uint32_t id) {
        return std::ranges::any_of(config.factions, [id](const auto& f) { return f.id == id; });
    };
    for (const auto& capital : config.capitals) {
        const std::string which =
            "config: the capital of faction " + std::to_string(capital.faction_id);
        if (!has_faction(capital.faction_id)) {
            throw std::runtime_error(which + ": no such faction");
        }
        if (capital.cell >= config.map_width * config.map_height) {
            throw std::runtime_error(which + " is off the map");
        }
    }
    for (const auto& faction : config.factions) {
        const auto count =
            std::ranges::count(config.capitals, faction.id, &CapitalConfig::faction_id);
        if (count != 1) {
            throw std::runtime_error("config: faction " + std::to_string(faction.id) +
                                     " needs exactly one capital");
        }
    }
    for (std::size_t i = 0; i < config.capitals.size(); ++i) {
        for (std::size_t j = i + 1; j < config.capitals.size(); ++j) {
            const auto& a = config.capitals[i];
            const auto& b = config.capitals[j];
            const std::int64_t dx =
                std::int64_t{a.cell % config.map_width} - b.cell % config.map_width;
            const std::int64_t dy =
                std::int64_t{a.cell / config.map_width} - b.cell / config.map_width;
            const std::int64_t reach = std::int64_t{a.protected_radius} + b.protected_radius;
            if (dx * dx + dy * dy <= reach * reach) {
                throw std::runtime_error("config: the zones of the capitals of factions " +
                                         std::to_string(a.faction_id) + " and " +
                                         std::to_string(b.faction_id) + " may overlap");
            }
        }
    }
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
    config.capture_enemy_multiplier = game.at("capture_enemy_multiplier");
    if (config.capture_enemy_multiplier <= 0.0) {
        throw std::runtime_error("config: capture_enemy_multiplier must be > 0");
    }
    config.player_radius = game.at("player_radius");
    config.vision_radius = game.at("vision_radius");
    config.season_id = game.at("season_id");
    if (config.season_id == 0) {
        throw std::runtime_error("config: season_id must be >= 1");
    }
    if (config.vision_radius == 0) {
        throw std::runtime_error("config: vision_radius must be > 0");
    }
    config.limits = parse_limits(game.at("limits"));

    for (const auto& f : game.at("factions")) {
        config.factions.push_back(FactionConfig{f.at("id"), f.at("name"), f.at("color")});
    }
    for (const auto& c : game.at("capitals")) {
        config.capitals.push_back(
            CapitalConfig{c.at("faction_id"), c.at("cell"), c.at("protected_radius")});
    }
    validate_capitals(config);
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
    spdlog::info("Game respawn_delay_ticks={} reconnect_grace_ms={} factions={} capitals={}",
                 game_config_.respawn_delay_ticks, game_config_.reconnect_grace_ms,
                 game_config_.factions.size(), game_config_.capitals.size());
    const LimitsConfig& l = game_config_.limits;
    spdlog::info(
        "Limits max_input_frames={} input_queue={} messages_per_second={} handshake_timeout_ms={} "
        "idle_timeout_ms={} resync_window_ms={}",
        l.max_input_frames, l.input_queue, l.messages_per_second, l.handshake_timeout_ms,
        l.idle_timeout_ms, l.resync_window_ms);
}

LogConfig parse_log_config(const std::string& config_json) {
    // spdlog's own names; spdlog::level::from_str would take anything else as "off".
    static constexpr std::array<std::string_view, 7> kLevels{"trace", "debug",    "info", "warn",
                                                             "error", "critical", "off"};
    const nlohmann::json doc = nlohmann::json::parse(config_json);
    const auto& log = doc.at("log");
    LogConfig config{log.at("level"), log.at("metrics_interval_s")};
    if (std::ranges::find(kLevels, config.level) == kLevels.end()) {
        throw std::runtime_error("config: unknown log level '" + config.level + "'");
    }
    if (config.metrics_interval_s == 0) {
        throw std::runtime_error("config: log.metrics_interval_s must be > 0");
    }
    return config;
}

void Config::init_log_config(const std::string& filename) {
    std::ifstream file(filename);
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    log_config_ = parse_log_config(text);

    spdlog::info("Log level={} metrics_interval_s={}", log_config_.level,
                 log_config_.metrics_interval_s);
}

SaveConfig parse_save_config(const std::string& config_json) {
    const nlohmann::json doc = nlohmann::json::parse(config_json);
    const auto& save = doc.at("save");
    SaveConfig config{save.at("dir"), save.at("interval_s"), save.at("keep")};
    if (config.dir.empty()) throw std::runtime_error("config: save.dir must not be empty");
    if (config.interval_s == 0) throw std::runtime_error("config: save.interval_s must be > 0");
    if (config.keep == 0) throw std::runtime_error("config: save.keep must be > 0");
    return config;
}

void Config::init_save_config(const std::string& filename) {
    std::ifstream file(filename);
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    save_config_ = parse_save_config(text);

    spdlog::info("Save dir={} interval_s={} keep={} season_id={}", save_config_.dir,
                 save_config_.interval_s, save_config_.keep, game_config_.season_id);
}
}  // namespace lit
