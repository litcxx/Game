#pragma once

#include <cstdint>
#include <string_view>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/vision.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// ServerMessage builders: pure functions of the state. World decides who gets what.

// Handshake reply: the joiner's player id (its character's), its session token,
// whether it resumed a character still in the world, the game rules and the
// factions.
::game::v1::ServerMessage make_welcome(const WorldState& state, const GameConfig& config,
                                       const Character& character, std::string_view session_token,
                                       bool resumed);
// The whole territory for a joiner as far as `vision` shows it: the owners and
// in-progress captures of visible cells; every other cell reads as 0 (unknown).
::game::v1::ServerMessage make_map_state(const WorldState& state, const Vision& vision);
// Every character in the world (id, name, faction), marked full: it replaces the
// client's roster (on join and on resync).
::game::v1::ServerMessage make_full_roster(const WorldState& state);
// One character joined or changed (e.g. picked a faction).
::game::v1::ServerMessage make_roster_upsert(const WorldState& state, const Character& character);
// One player left the world (its reconnect grace is over).
::game::v1::ServerMessage make_roster_removed(std::uint32_t player_id);
// Echo of a Ping (RTT on the client) with the current server tick.
::game::v1::ServerMessage make_pong(const WorldState& state, std::uint32_t client_time_ms);

// The faction scores (GDD 7.3): for each faction, in the config's order, the
// cells it owns and its characters in the world (as in the roster) — the real
// count, not under fog.
::game::v1::ServerMessage make_faction_scores(const WorldState& state, const GameConfig& config);

// Whether an error closes the connection: codes 1–19 (connection and protocol)
// always do, 20+ (a refused request) never do — see ErrorCode in protocol.proto.
bool is_fatal(::game::v1::ErrorCode code);
// A ServerError answering ClientMessage.request_id (0 = none); `detail` is for
// logs, not for the player.
::game::v1::ServerMessage make_error(::game::v1::ErrorCode code, std::uint32_t request_id,
                                     std::string_view detail);

// One cell's owner and capture state (shared by MapState and Snapshot).
void fill_cell_update(::game::v1::CellUpdate* out, const Territory& territory, std::uint32_t index);
}  // namespace lit::game
