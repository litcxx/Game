#pragma once

#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// Queue a client's input frames in seq order (duplicates / out-of-order frames are
// dropped). Frames are queued even when the player is not alive, so
// last_input_seq (the client's ack) keeps advancing.
void enqueue_frames(Player& player, const ::game::v1::Input& input);

// Pop one queued command per player and make it the current intent; with an empty
// queue the last intent repeats. Sets last_input_seq (the ack for reconciliation).
void consume_inputs(WorldState& state);
}  // namespace lit::game
