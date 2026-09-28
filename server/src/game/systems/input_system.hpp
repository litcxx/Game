#pragma once

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// Queue a client's input frames in seq order (duplicates / out-of-order frames are
// dropped). At most limits.max_input_frames are taken from one Input and at most
// limits.input_queue are kept; the frames beyond are dropped — a burst after a
// stall costs only a prediction correction, never the connection. Frames are
// queued even when the player is not alive, so last_input_seq (the client's ack)
// keeps advancing.
void enqueue_frames(Player& player, const ::game::v1::Input& input, const LimitsConfig& limits);

// Pop one queued command per player and make it the current intent; with an empty
// queue the last intent repeats. Sets last_input_seq (the ack for reconciliation).
void consume_inputs(WorldState& state);
}  // namespace lit::game
