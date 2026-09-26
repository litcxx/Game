#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "net/i_client_gateway.hpp"

namespace lit::test {
// Captures everything the game loop sends, so World's outbound behaviour can be
// asserted in unit tests without a real network.
class MockClientGateway : public lit::IClientGateway {
  public:
    struct Sent {
        std::uint64_t session_id;
        std::vector<std::byte> bytes;
    };

    std::vector<Sent> sent;
    std::vector<std::vector<std::byte>> broadcasts;

    void send_to(std::uint64_t session_id, std::vector<std::byte> bytes) override {
        sent.push_back({session_id, std::move(bytes)});
    }
    void broadcast(std::vector<std::byte> bytes) override {
        broadcasts.push_back(std::move(bytes));
    }
};
}  // namespace lit::test
