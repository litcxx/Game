#pragma once

#include <cstdint>
#include <deque>

#include "state/unit.hpp"

namespace lit::game {
// One tick's worth of a player's intent, consumed one per tick for deterministic
// replay.
struct InputCommand {
    std::uint32_t seq{0};
    Intent intent;
};

// The input one client connection sent: commands waiting for their tick, in seq
// order. Seqs are the client's own numbering on that connection, so a new
// connection starts a new queue.
struct InputQueue {
    std::deque<InputCommand> commands;
    std::uint32_t last_enqueued_seq{0};  // highest seq accepted into the queue
    std::uint32_t last_input_seq{0};     // seq of the command applied last (the client's ack)
};
}  // namespace lit::game
