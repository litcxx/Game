#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "game/v1/protocol.pb.h"

namespace lit::game {
// Whether a connection may join the world: its Hello speaks this protocol
// version (PROTOCOL_VERSION_CURRENT) and gives an acceptable name. Returns the
// name to use, or the refusal (PROTOCOL_VERSION or INVALID_NAME, both fatal).
std::expected<std::string, ::game::v1::ErrorCode> check_hello(const ::game::v1::Hello& hello);

// A player name as shown to everyone: `raw` trimmed of surrounding spaces,
// accepted when it is 1–16 characters (code points, not bytes) of valid UTF-8
// with no control or invisible characters; nullopt otherwise.
std::optional<std::string> normalize_name(std::string_view raw);
}  // namespace lit::game
