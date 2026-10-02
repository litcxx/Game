#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "game/v1/protocol.pb.h"
#include "state/character.hpp"
#include "state/client_session.hpp"
#include "state/faction_memory.hpp"
#include "state/projectile.hpp"
#include "state/territory.hpp"
#include "state/unit.hpp"

namespace lit::game {
// All mutable simulation state, as plain data. Systems read and write it; the
// World orchestrates the tick and owns the network side.
struct WorldState {
    std::uint32_t tick{0};
    std::int64_t season_started_at{0};   // unix seconds: when this season's world began
    std::uint32_t next_character_id{1};  // player ids: never reused
    std::uint32_t next_projectile_id{1};
    // Who plays (identity), what fights (bodies in the world) and who is
    // connected: a session drives a character, whose body is the unit with the
    // character's id.
    std::unordered_map<std::uint32_t, Character> characters;    // key: character id (player_id)
    std::unordered_map<std::uint32_t, Unit> units;              // key: unit id
    std::unordered_map<std::uint64_t, ClientSession> sessions;  // key: session_id
    Territory territory;
    // What each faction has seen of the map this season (one sight, one memory).
    std::unordered_map<std::uint32_t, FactionMemory> memory;  // key: faction id
    std::vector<Projectile> projectiles;                      // in flight, in launch order
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

// A character's faction, locked at its first spawn — with or without a body now
// (dead, or back after leaving the world); 0 before its first spawn.
inline std::uint32_t faction_of(const WorldState& state, std::uint32_t character_id) {
    auto it = state.characters.find(character_id);
    return it == state.characters.end() ? 0 : it->second.faction_id;
}
}  // namespace lit::game
