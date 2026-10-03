#pragma once

#include <cstdint>
#include <deque>

#include "state/unit.hpp"

namespace lit::game {
// The input one client connection sent: intents waiting for their tick, one per
// tick, in input_seq order. Input seqs are the client's own numbering on that
// connection, so a new connection starts a new queue.
struct InputQueue {
    std::deque<Intent> intents;
    std::uint32_t last_enqueued_input_seq{0};  // highest input_seq accepted into the queue
    std::uint32_t last_input_seq{0};           // input_seq of the intent applied last (the ack)
};
}  // namespace lit::game
