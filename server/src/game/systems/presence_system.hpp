#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "state/world_state.hpp"
#include "utils/session_token.hpp"

namespace lit::game {
// Who is in the world and which connection drives whom. A character joins with
// a new id and no body; a session drives one character. A dropped character
// stays in the world, away, until its reconnect grace ends, then leaves with
// its body; its record stays, and a session with its token brings it back.

// A new character named `name` with the next character id — its public
// player_id, never reused — and the hash of its session token. It has no body
// until it spawns.
Character& create_character(WorldState& state, std::string name, const TokenHash& token_hash);

// `session_id` drives `character_id` from now on, with a fresh input queue and
// nothing told yet: its first snapshot reveals everything it sees. The
// character is in the world again, no longer away. Returns the session that
// drove it until now, if any — it drives nothing any more.
std::optional<std::uint64_t> attach_session(WorldState& state, std::uint64_t session_id,
                                            std::uint32_t character_id);

// `session_id` stops driving its character, which stays in the world, away,
// until `away_until`; its body stands still. Returns that character's id, or
// nullopt for a session that drives none.
std::optional<std::uint32_t> detach_session(WorldState& state, std::uint64_t session_id,
                                            std::uint32_t away_until);

// The character leaves the world, its body (alive or dead) with it; its record
// stays.
void leave_world(WorldState& state, std::uint32_t character_id);

// Every character away until `tick` or earlier leaves the world. Returns their
// ids, in ascending order.
std::vector<std::uint32_t> leave_after_grace(WorldState& state, std::uint32_t tick);

// The character with this token hash, in the world or not; nullptr if none.
Character* find_by_token(WorldState& state, const TokenHash& token_hash);
}  // namespace lit::game
