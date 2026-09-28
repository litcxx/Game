#pragma once

#include <cstdint>

#include "state/client_sync.hpp"

namespace lit::game {
// What to do with a client whose frame was just dropped.
enum class DropVerdict : std::uint8_t {
    Resync,  // resend what it may have missed in the next snapshot
    Close,   // it keeps falling behind: give up on the connection
};

// A frame to this client was dropped on `tick`. The first drop asks for a resync
// (sync.resync); a drop on a later tick less than `window_ticks` after the last
// one closes. Drops on one tick are one drop — a full queue refuses them all.
DropVerdict note_drop(ClientSync& sync, std::uint32_t tick, std::uint32_t window_ticks);
}  // namespace lit::game
