#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// Where a character spawning in `faction_id` comes into the world: a cell (its
// body stands at the centre), or nullopt when the faction has nowhere to spawn.
// The World is given one: the capital rule below in play; tests place bodies
// where their case needs them.
using SpawnPoint = std::function<std::optional<std::uint32_t>(
    const WorldState& state, std::uint32_t character_id, std::uint32_t faction_id)>;

// The rule until fortresses (GAME-016): the faction's capital cell.
SpawnPoint capital_spawn_point(const GameConfig& config);

// Validate and apply a character's spawn request (the character exists).
// Refused — with the reason — when its body is alive (ALREADY_SPAWNED), still
// waiting out its respawn delay (SPAWN_TOO_EARLY), or the faction is unknown, not
// the one the character is locked to, or has nowhere to spawn (INVALID_FACTION).
// `spawn_point` is asked only once nothing else refuses. The first successful
// spawn locks the character's faction for the season (GDD 7.11). On success its
// body — the unit with the character's id — is placed anew at the centre of the
// spawn cell: alive, full hp, no intent, no cooldowns.
std::expected<void, ::game::v1::ErrorCode> try_spawn(WorldState& state, const GameConfig& config,
                                                     std::uint32_t character_id,
                                                     std::uint32_t faction_id,
                                                     const SpawnPoint& spawn_point);
}  // namespace lit::game
