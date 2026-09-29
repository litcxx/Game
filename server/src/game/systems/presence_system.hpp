#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "state/world_state.hpp"

namespace lit::game {
// Who is in the world and which connection drives whom. A character joins with
// a new id and no body; a session drives one character; a character leaves with
// its body. The World joins on Hello and leaves on disconnect; sessions and
// characters are kept apart so that another connection can take a character over.

// A new character named `name` with the next character id — its public
// player_id, never reused. It has no body until it spawns.
Character& create_character(WorldState& state, std::string name);

// `session_id` drives `character_id` from now on, with a fresh input queue and
// nothing told yet: its first snapshot reveals everything it sees.
void attach_session(WorldState& state, std::uint64_t session_id, std::uint32_t character_id);

// `session_id` stops driving its character, which stays in the world. Returns
// that character's id, or nullopt for a session that drives none.
std::optional<std::uint32_t> detach_session(WorldState& state, std::uint64_t session_id);

// The character leaves the world, its body (alive or dead) with it.
void remove_character(WorldState& state, std::uint32_t character_id);
}  // namespace lit::game
