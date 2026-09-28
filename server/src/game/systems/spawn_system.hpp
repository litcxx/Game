#pragma once

#include <cstdint>
#include <expected>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// Validate and apply a spawn request. Refused — with the reason — when the
// player is alive (ALREADY_SPAWNED), still waiting out its respawn delay
// (SPAWN_TOO_EARLY), or the cell (SPAWN_INVALID_CELL) / faction (INVALID_FACTION)
// is invalid. On success the player is placed at the cell centre with full hp
// and cleared intent/cooldowns.
std::expected<void, ::game::v1::ErrorCode> try_spawn(const WorldState& state,
                                                     const GameConfig& config, Player& player,
                                                     std::uint32_t cell, std::uint32_t faction_id);
}  // namespace lit::game
