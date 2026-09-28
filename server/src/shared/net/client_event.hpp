#pragma once

#include <cstdint>

#include "game/v1/protocol.pb.h"

namespace lit {
// A single ordered event on the net→game channel. `Connected` opens a session
// (ahead of all its messages), `Message` carries a parsed client frame, and
// `Disconnected` signals the session ended (its earlier messages are already
// ahead of it in the queue, so per-session order is preserved).
struct ClientEvent {
    enum class Kind : std::uint8_t { Message, Disconnected, Connected };

    std::uint64_t session_id;
    Kind kind;
    ::game::v1::ClientMessage msg;  // valid only when kind == Message
};
}  // namespace lit
