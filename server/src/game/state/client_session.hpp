#pragma once

#include <cstdint>

#include "state/client_sync.hpp"
#include "state/input_queue.hpp"

namespace lit::game {
// A connected client and the character it drives: the input it sends and what
// it has been told. Both belong to the connection, so another session driving
// the same character starts them afresh.
struct ClientSession {
    std::uint32_t character_id{0};
    InputQueue input;
    ClientSync sync;
};
}  // namespace lit::game
