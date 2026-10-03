#include "systems/presence_system.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lit::game {
Character& create_character(WorldState& state, std::string name, const TokenHash& token_hash) {
    const std::uint32_t id = state.next_character_id++;
    return state.characters[id] = Character{id, std::move(name), token_hash, true, std::nullopt};
}

std::optional<std::uint64_t> attach_session(WorldState& state, std::uint64_t session_id,
                                            std::uint32_t character_id) {
    std::optional<std::uint64_t> replaced;
    for (const auto& [sid, session] : state.sessions) {
        if (sid != session_id && session.character_id == character_id) replaced = sid;
    }
    if (replaced) state.sessions.erase(*replaced);
    state.sessions[session_id] = ClientSession{character_id, {}, {}};
    // The new connection numbers its input frames from 1 again: nothing of the
    // character's refers to the old connection's frames any more.
    if (Unit* body = find_unit(state, character_id)) body->last_attack_input_seq = 0;
    for (Projectile& p : state.projectiles) {
        if (p.owner_id == character_id) p.input_seq = 0;
    }
    Character& character = state.characters.at(character_id);
    character.in_world = true;
    character.away_until.reset();
    return replaced;
}

std::optional<std::uint32_t> detach_session(WorldState& state, std::uint64_t session_id,
                                            std::uint32_t away_until) {
    auto it = state.sessions.find(session_id);
    if (it == state.sessions.end()) return std::nullopt;
    const std::uint32_t character_id = it->second.character_id;
    state.sessions.erase(it);
    state.characters.at(character_id).away_until = away_until;
    // No one drives it now: without a fresh command its last intent would repeat.
    if (Unit* body = find_unit(state, character_id)) body->intent = {};
    return character_id;
}

void leave_world(WorldState& state, std::uint32_t character_id) {
    state.units.erase(character_id);
    Character& character = state.characters.at(character_id);
    character.in_world = false;
    character.away_until.reset();
}

std::vector<std::uint32_t> leave_after_grace(WorldState& state, std::uint32_t tick) {
    std::vector<std::uint32_t> left;
    for (const auto& [id, character] : state.characters) {
        if (character.away_until && *character.away_until <= tick) left.push_back(id);
    }
    std::ranges::sort(left);
    for (std::uint32_t id : left) leave_world(state, id);
    return left;
}

Character* find_by_token(WorldState& state, const TokenHash& token_hash) {
    for (auto& [id, character] : state.characters) {
        if (character.token_hash == token_hash) return &character;
    }
    return nullptr;
}
}  // namespace lit::game
