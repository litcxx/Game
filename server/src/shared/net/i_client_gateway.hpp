#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lit {
// The seam between the game loop and the network layer. The game loop hands
// already-serialized frames to an IClientGateway; net::Server implements it on
// top of the per-session channels. Keeping this abstract lets World stay free of
// transport details and be unit-tested against a mock gateway.
class IClientGateway {
  public:
    virtual ~IClientGateway() = default;

    // Deliver a frame to one session. Non-blocking; dropped if the session is
    // unknown or its send queue is full. Safe to call from the game thread.
    virtual void send_to(std::uint64_t session_id, std::vector<std::byte> bytes) = 0;

    // Deliver a frame to every connected session.
    virtual void broadcast(std::vector<std::byte> bytes) = 0;
};
}  // namespace lit
