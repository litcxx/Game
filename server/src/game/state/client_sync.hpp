#pragma once

#include <cstdint>

#include "state/vision.hpp"

namespace lit::game {
// What the server has told one connected client — the base for its snapshot
// deltas — and how delivery to it is going. Transient, per connection; what a
// player has explored belongs to the player, not here.
struct ClientSync {
    Vision vision;               // the cells this client was last told it sees
    bool resync{false};          // a frame to it was dropped: the next snapshot resyncs
    std::uint32_t drop_tick{0};  // the tick of the last dropped frame (0 = none yet)
};
}  // namespace lit::game
