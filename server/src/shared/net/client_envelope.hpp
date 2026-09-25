#pragma once

#include <cstdint>

#include "game/v1/protocol.pb.h"

namespace lit {
// An inbound client message tagged with the session it arrived on, so the game
// loop can address replies back to the sender.
struct ClientEnvelope {
    std::uint64_t session_id;
    ::game::v1::ClientMessage msg;
};
}  // namespace lit
