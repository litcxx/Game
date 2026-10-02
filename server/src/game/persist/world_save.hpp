#pragma once

#include <cstdint>
#include <expected>
#include <string>

#include "config/config.hpp"
#include "save.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// The world save's format (server/proto/save.proto). Raised only when an older
// server could no longer read a newer file right; fields are just added.
inline constexpr std::uint32_t kWorldSaveVersion = 1;

// How a save came back: the same season goes on, or a new one begins.
enum class SeasonLoad : std::uint8_t { Continued, New };

// The world's persistent part (GAME-019): the season, the next player id, every
// cell's owner and every character — id, name, token hash, faction — by id.
// Transient state (bodies, capture progress, projectiles, sessions) is not in it.
::lit::save::WorldSave make_world_save(const WorldState& state, const GameConfig& config);

// Brings a save back into `state`, a world as the World makes it before anyone
// joins (the map, the capitals' zones seeded and protected). Every character
// comes back out of the world.
//   - The config's season: the owners, the factions and the season's start too.
//   - Another season: a new one begins — the characters stay (id, name, token),
//     their season's data goes (the faction), the territory starts again from
//     the capitals and the season starts with this world.
// A save this server can't take — a newer format, another map or an unknown
// faction in the same season, bad ids or token hashes — is refused with why,
// and `state` is left as it was.
std::expected<SeasonLoad, std::string> restore_world(WorldState& state, const GameConfig& config,
                                                     const ::lit::save::WorldSave& save);
}  // namespace lit::game
