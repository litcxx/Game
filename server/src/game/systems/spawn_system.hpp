#pragma once

#include <cstdint>
#include <expected>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// Validate and apply a character's spawn request (the character exists).
// Refused — with the reason — when its body is alive (ALREADY_SPAWNED), still
// waiting out its respawn delay (SPAWN_TOO_EARLY), the cell is off the map
// (SPAWN_INVALID_CELL), or the faction is unknown or not the one the character
// is locked to (INVALID_FACTION). The first successful spawn locks the character's
// faction for the season (GDD 7.11). On success its body — the unit with the
// character's id — is placed anew at the cell centre: alive, full hp, no intent,
// no cooldowns.
std::expected<void, ::game::v1::ErrorCode> try_spawn(WorldState& state, const GameConfig& config,
                                                     std::uint32_t character_id, std::uint32_t cell,
                                                     std::uint32_t faction_id);
}  // namespace lit::game
