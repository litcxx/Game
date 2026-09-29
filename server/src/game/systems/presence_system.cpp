#include "systems/presence_system.hpp"

#include <utility>

namespace lit::game {
Character& create_character(WorldState& state, std::string name) {
    const std::uint32_t id = state.next_character_id++;
    return state.characters[id] = Character{id, std::move(name)};
}

void attach_session(WorldState& state, std::uint64_t session_id, std::uint32_t character_id) {
    state.sessions[session_id] = ClientSession{character_id, {}, {}};
}

std::optional<std::uint32_t> detach_session(WorldState& state, std::uint64_t session_id) {
    auto it = state.sessions.find(session_id);
    if (it == state.sessions.end()) return std::nullopt;
    const std::uint32_t character_id = it->second.character_id;
    state.sessions.erase(it);
    return character_id;
}

void remove_character(WorldState& state, std::uint32_t character_id) {
    state.units.erase(character_id);
    state.characters.erase(character_id);
}
}  // namespace lit::game
