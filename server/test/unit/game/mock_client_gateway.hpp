#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "net/i_client_gateway.hpp"

namespace lit::test {
// Captures everything the game loop sends, so World's outbound behaviour can be
// asserted in unit tests without a real network. `accept` can refuse frames, as
// a full send queue does.
class MockClientGateway : public lit::IClientGateway {
  public:
    struct Sent {
        std::uint64_t session_id;
        std::vector<std::byte> bytes;
    };

    std::vector<Sent> sent;      // delivered frames
    std::vector<Sent> rejected;  // frames `accept` refused (dropped)
    std::vector<std::vector<std::byte>> broadcasts;
    std::vector<std::uint64_t> disconnected;  // sessions the game loop closed
    // Whether a frame is delivered; unset = every frame is.
    std::function<bool(std::uint64_t session_id, const std::vector<std::byte>& bytes)> accept;

    bool send_to(std::uint64_t session_id, std::vector<std::byte> bytes) override {
        if (accept && !accept(session_id, bytes)) {
            rejected.push_back({session_id, std::move(bytes)});
            return false;
        }
        sent.push_back({session_id, std::move(bytes)});
        return true;
    }
    void broadcast(std::vector<std::byte> bytes) override {
        broadcasts.push_back(std::move(bytes));
    }
    void disconnect(std::uint64_t session_id) override { disconnected.push_back(session_id); }
};
}  // namespace lit::test
