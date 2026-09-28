#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "game/v1/protocol.pb.h"
#include "state/player.hpp"
#include "state/projectile.hpp"
#include "state/territory.hpp"

namespace lit::game {
// All mutable simulation state, as plain data. Systems read and write it; the
// World orchestrates the tick and owns the network side.
struct WorldState {
    std::uint32_t tick{0};
    std::uint32_t next_player_id{1};
    std::uint32_t next_projectile_id{1};
    std::unordered_map<std::uint64_t, Player> players;  // key: session_id
    Territory territory;
    std::vector<Projectile> projectiles;  // in flight, in launch order
    // Combat events (hits/deaths/ability uses) since the last snapshot; flushed to
    // each recipient's Snapshot.events as far as it sees them, then cleared.
    std::vector<::game::v1::GameEvent> events;
};
}  // namespace lit::game
