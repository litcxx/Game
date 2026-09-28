#pragma once

#include <cstdint>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/vision.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// ServerMessage builders: pure functions of the state. World decides who gets what.

// Handshake reply: the joiner's player id, the game rules and the factions.
::game::v1::ServerMessage make_welcome(const WorldState& state, const GameConfig& config,
                                       const Player& player);
// The whole territory for a joiner as far as `vision` shows it: the owners and
// in-progress captures of visible cells; every other cell reads as 0 (unknown).
::game::v1::ServerMessage make_map_state(const WorldState& state, const Vision& vision);
// Every connected player (id, name, faction).
::game::v1::ServerMessage make_full_roster(const WorldState& state);
// One player joined or changed (e.g. picked a faction).
::game::v1::ServerMessage make_roster_upsert(const Player& player);
// One player left.
::game::v1::ServerMessage make_roster_removed(std::uint32_t player_id);
// Echo of a Ping (RTT on the client) with the current server tick.
::game::v1::ServerMessage make_pong(const WorldState& state, std::uint32_t client_time_ms);

// One cell's owner and capture state (shared by MapState and Snapshot).
void fill_cell_update(::game::v1::CellUpdate* out, const Territory& territory, std::uint32_t index);
}  // namespace lit::game
