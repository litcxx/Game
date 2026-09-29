#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "game/v1/protocol.pb.h"
#include "state/character.hpp"
#include "state/client_session.hpp"
#include "state/projectile.hpp"
#include "state/territory.hpp"
#include "state/unit.hpp"

namespace lit::game {
// All mutable simulation state, as plain data. Systems read and write it; the
// World orchestrates the tick and owns the network side.
struct WorldState {
    std::uint32_t tick{0};
    std::uint32_t next_character_id{1};  // player ids: never reused
    std::uint32_t next_projectile_id{1};
    // Who plays (identity), what fights (bodies in the world) and who is
    // connected: a session drives a character, whose body is the unit with the
    // character's id.
    std::unordered_map<std::uint32_t, Character> characters;    // key: character id (player_id)
    std::unordered_map<std::uint32_t, Unit> units;              // key: unit id
    std::unordered_map<std::uint64_t, ClientSession> sessions;  // key: session_id
    Territory territory;
    std::vector<Projectile> projectiles;  // in flight, in launch order
    // Combat events (hits/deaths/ability uses) since the last snapshot; flushed to
    // each recipient's Snapshot.events as far as it sees them, then cleared.
    std::vector<::game::v1::GameEvent> events;
};

// The unit with this id, or nullptr — e.g. a character that has not spawned.
inline Unit* find_unit(WorldState& state, std::uint32_t id) {
    auto it = state.units.find(id);
    return it == state.units.end() ? nullptr : &it->second;
}
inline const Unit* find_unit(const WorldState& state, std::uint32_t id) {
    auto it = state.units.find(id);
    return it == state.units.end() ? nullptr : &it->second;
}

// A character's faction: its body's, or 0 while it has none (not spawned yet).
inline std::uint32_t faction_of(const WorldState& state, std::uint32_t character_id) {
    const Unit* unit = find_unit(state, character_id);
    return unit != nullptr ? unit->faction_id : 0;
}
}  // namespace lit::game
