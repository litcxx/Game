#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// Whether a connection may join the world: its Hello speaks this protocol
// version (PROTOCOL_VERSION_CURRENT) and gives an acceptable name. Returns the
// name to use, or the refusal (PROTOCOL_VERSION or INVALID_NAME, both fatal).
std::expected<std::string, ::game::v1::ErrorCode> check_hello(const ::game::v1::Hello& hello);

// A player name as shown to everyone: `raw` trimmed of surrounding spaces,
// accepted when it is 1–16 characters (code points, not bytes) of valid UTF-8
// with no control or invisible characters; nullopt otherwise.
std::optional<std::string> normalize_name(std::string_view raw);

// Whether a character — in the world or not — already has this name, ignoring
// case: Latin and Cyrillic letters (А–Я, Ё, Ѐ–Џ) match either case. A new
// player may not take it (INVALID_NAME); temporary until accounts (GDD 7.1).
bool is_name_taken(const WorldState& state, std::string_view name);
}  // namespace lit::game
