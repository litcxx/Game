#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "game/v1/protocol.pb.h"
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
    // Sessions the game loop closed, with the reason (UNSPECIFIED = a plain close).
    std::vector<std::pair<std::uint64_t, ::game::v1::ErrorCode>> disconnected;
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
    void disconnect(std::uint64_t session_id, ::game::v1::ErrorCode reason) override {
        disconnected.emplace_back(session_id, reason);
    }
};
}  // namespace lit::test
