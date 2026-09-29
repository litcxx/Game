#include <gtest/gtest.h>

#include <exception>
#include <string>

#include "config/config.hpp"

namespace {

// The connection limits as config.json has them.
const std::string kLimits = R"({ "max_input_frames": 8, "input_queue": 32,
    "messages_per_second": 120, "handshake_timeout_ms": 5000, "idle_timeout_ms": 20000,
    "resync_window_ms": 5000 })";

// A complete config document with `abilities`, the vision radius and the limits
// spliced into the game section.
std::string config_json(const std::string& abilities, const std::string& vision_radius = "400",
                        const std::string& limits = kLimits) {
    return R"({
      "server": { "ip": "0.0.0.0", "port": 27998, "io_threads": 1 },
      "game": {
        "tick_rate": 60, "snapshot_rate": 20, "map_width": 100, "map_height": 100,
        "move_speed": 300, "max_hp": 100, "respawn_delay_ticks": 300,
        "reconnect_grace_ms": 30000, "capture_ticks": 60, "player_radius": 16,
        "vision_radius": )" +
           vision_radius + R"(, "limits": )" + limits + R"(,
        "factions": [ { "id": 1, "name": "Red", "color": 16711680 } ],
        "abilities": )" +
           abilities + R"(
      }
    })";
}

const std::string kMelee =
    R"({ "id": 1, "kind": "melee", "name": "Удар", "cooldown_ticks": 45, "damage": 20, "range": 120 })";

}  // namespace

TEST(GameConfigParse, ReadsAbilitiesInBarOrder) {
    const auto c = lit::parse_game_config(config_json("[" + kMelee + R"(,
        { "id": 2, "kind": "projectile", "name": "Выстрел", "cooldown_ticks": 90, "damage": 30,
          "range": 500, "projectile_speed": 800, "projectile_radius": 8 } ])"));

    EXPECT_EQ(c.player_radius, 16u);
    ASSERT_EQ(c.abilities.size(), 2u);
    const auto& melee = c.abilities[0];
    EXPECT_EQ(melee.id, 1u);
    EXPECT_EQ(melee.kind, lit::AbilityKind::Melee);
    EXPECT_EQ(melee.name, "Удар");
    EXPECT_EQ(melee.cooldown_ticks, 45u);
    EXPECT_EQ(melee.damage, 20u);
    EXPECT_EQ(melee.range, 120u);
    EXPECT_EQ(melee.projectile_speed, 0u);  // absent for melee -> 0
    EXPECT_EQ(melee.projectile_radius, 0u);
    const auto& shot = c.abilities[1];
    EXPECT_EQ(shot.id, 2u);
    EXPECT_EQ(shot.kind, lit::AbilityKind::Projectile);
    EXPECT_EQ(shot.name, "Выстрел");
    EXPECT_EQ(shot.cooldown_ticks, 90u);
    EXPECT_EQ(shot.damage, 30u);
    EXPECT_EQ(shot.range, 500u);
    EXPECT_EQ(shot.projectile_speed, 800u);
    EXPECT_EQ(shot.projectile_radius, 8u);
}

TEST(GameConfigParse, RequiresAtLeastOneAbility) {
    EXPECT_THROW(lit::parse_game_config(config_json("[]")), std::exception);
}

TEST(GameConfigParse, RejectsUnknownAbilityKind) {
    EXPECT_THROW(lit::parse_game_config(config_json(
                     R"([{ "id": 1, "kind": "laser", "name": "Zap", "cooldown_ticks": 45,
                           "damage": 20, "range": 120 }])")),
                 std::exception);
}

TEST(GameConfigParse, RejectsZeroOrDuplicateAbilityIds) {
    EXPECT_THROW(lit::parse_game_config(config_json(
                     R"([{ "id": 0, "kind": "melee", "name": "A", "cooldown_ticks": 45,
                           "damage": 20, "range": 120 }])")),
                 std::exception);
    EXPECT_THROW(lit::parse_game_config(config_json("[" + kMelee + "," + kMelee + "]")),
                 std::exception);
}

TEST(GameConfigParse, RejectsProjectileThatCannotFly) {
    // Speed 0 would never use up its range: it must be rejected, not loaded.
    EXPECT_THROW(lit::parse_game_config(config_json(
                     R"([{ "id": 1, "kind": "projectile", "name": "Dud", "cooldown_ticks": 90,
                           "damage": 30, "range": 500, "projectile_speed": 0,
                           "projectile_radius": 8 }])")),
                 std::exception);
}

TEST(GameConfigParse, ReadsBlockAbility) {
    const auto c = lit::parse_game_config(config_json("[" + kMelee + R"(,
        { "id": 3, "kind": "block", "name": "Блок", "cooldown_ticks": 45, "duration_ticks": 9 } ])"));

    ASSERT_EQ(c.abilities.size(), 2u);
    const auto& block = c.abilities[1];
    EXPECT_EQ(block.id, 3u);
    EXPECT_EQ(block.kind, lit::AbilityKind::Block);
    EXPECT_EQ(block.name, "Блок");
    EXPECT_EQ(block.cooldown_ticks, 45u);
    EXPECT_EQ(block.duration_ticks, 9u);
    EXPECT_EQ(block.damage, 0u);  // a block needs no damage or range
    EXPECT_EQ(block.range, 0u);
    EXPECT_EQ(c.abilities[0].duration_ticks, 0u);  // melee has no duration
}

TEST(GameConfigParse, ReadsVisionRadius) {
    const auto c = lit::parse_game_config(config_json("[" + kMelee + "]", "400"));

    EXPECT_EQ(c.vision_radius, 400u);
}

TEST(GameConfigParse, RejectsZeroVisionRadius) {
    // Nobody would see anything, not even their own cell: a broken config.
    EXPECT_THROW(lit::parse_game_config(config_json("[" + kMelee + "]", "0")), std::exception);
}

TEST(GameConfigParse, ReadsTheLimits) {
    const auto c = lit::parse_game_config(config_json("[" + kMelee + "]"));

    EXPECT_EQ(c.limits.max_input_frames, 8u);
    EXPECT_EQ(c.limits.input_queue, 32u);
    EXPECT_EQ(c.limits.messages_per_second, 120u);
    EXPECT_EQ(c.limits.handshake_timeout_ms, 5000u);
    EXPECT_EQ(c.limits.idle_timeout_ms, 20000u);
    EXPECT_EQ(c.limits.resync_window_ms, 5000u);
}

TEST(GameConfigParse, RejectsAZeroOrMissingLimit) {
    // A zero limit would refuse every frame or message, or never time anything
    // out — a broken config, not a choice.
    for (const std::string key : {"max_input_frames", "input_queue", "messages_per_second",
                                  "handshake_timeout_ms", "idle_timeout_ms", "resync_window_ms"}) {
        std::string zero = kLimits;
        const auto at = zero.find(':', zero.find(key));
        zero.replace(at + 1, zero.find_first_of(",}", at) - at - 1, " 0");
        EXPECT_THROW(lit::parse_game_config(config_json("[" + kMelee + "]", "400", zero)),
                     std::exception)
            << key << " = 0";

        std::string missing = kLimits;
        missing.replace(missing.find(key) - 1, key.size() + 2, "\"unused\"");
        EXPECT_THROW(lit::parse_game_config(config_json("[" + kMelee + "]", "400", missing)),
                     std::exception)
            << key << " missing";
    }
}

TEST(GameConfigParse, RejectsBlockWithoutDuration) {
    EXPECT_THROW(lit::parse_game_config(config_json(
                     R"([{ "id": 3, "kind": "block", "name": "Блок", "cooldown_ticks": 45 }])")),
                 std::exception);
    EXPECT_THROW(lit::parse_game_config(config_json(
                     R"([{ "id": 3, "kind": "block", "name": "Блок", "cooldown_ticks": 45,
                           "duration_ticks": 0 }])")),
                 std::exception);
}

namespace {

// A config document whose "log" section is `log` (omitted when empty).
std::string with_log(const std::string& log) {
    return R"({ "server": { "ip": "0.0.0.0", "port": 27998, "io_threads": 1 })" +
           (log.empty() ? std::string{} : R"(, "log": )" + log) + " }";
}

}  // namespace

TEST(LogConfigParse, ReadsTheLevelAndTheMetricsInterval) {
    const auto c =
        lit::parse_log_config(with_log(R"({ "level": "warn", "metrics_interval_s": 60 })"));

    EXPECT_EQ(c.level, "warn");
    EXPECT_EQ(c.metrics_interval_s, 60u);
}

TEST(LogConfigParse, TakesEveryLevelName) {
    for (const std::string level : {"trace", "debug", "info", "warn", "error", "critical", "off"}) {
        EXPECT_EQ(lit::parse_log_config(
                      with_log(R"({ "level": ")" + level + R"(", "metrics_interval_s": 1 })"))
                      .level,
                  level);
    }
}

TEST(LogConfigParse, RejectsAnUnknownLevel) {
    EXPECT_THROW(
        lit::parse_log_config(with_log(R"({ "level": "loud", "metrics_interval_s": 60 })")),
        std::exception);
}

TEST(LogConfigParse, RejectsAZeroMetricsInterval) {
    EXPECT_THROW(lit::parse_log_config(with_log(R"({ "level": "info", "metrics_interval_s": 0 })")),
                 std::exception);
}

TEST(LogConfigParse, RequiresTheSection) {
    EXPECT_THROW(lit::parse_log_config(with_log("")), std::exception);
    EXPECT_THROW(lit::parse_log_config(with_log(R"({ "level": "info" })")), std::exception);
}
