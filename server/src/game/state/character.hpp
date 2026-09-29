#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "utils/session_token.hpp"

namespace lit::game {
// A player's identity in the world, apart from any connection. `id` is the
// public player_id, never reused; its body, once spawned, is the Unit with the
// same id. A session drives it; dropped, it stays in the world (away) for the
// reconnect grace, then leaves — the record stays, for its token to bring it back.
struct Character {
    std::uint32_t id{0};
    std::string name;
    TokenHash token_hash{};  // SHA-256 of its session token; the token itself is never kept
    bool in_world{true};     // false once it has left; back when a session drives it again
    std::optional<std::uint32_t> away_until;  // no session drives it: leaves on this tick
};
}  // namespace lit::game
