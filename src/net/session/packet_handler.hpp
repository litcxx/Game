#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol/net_packet.hpp"

namespace lit::net {
class PacketHandler {
  public:
    PacketHandler() = default;
    ~PacketHandler() = default;
    PacketHandler(const PacketHandler&) = delete;
    PacketHandler& operator=(const PacketHandler&) = delete;

    // Get packet handler data.
    std::size_t head_already_read() const noexcept { return head_already_read_; }
    std::size_t body_already_read() const noexcept { return body_already_read_; }

    // Return pointer to current position in header buffer.
    std::uint8_t* head_current_data() noexcept {
        return packet_.get_head_data() + head_already_read_;
    }

    // Return pointer to current position in body buffer.
    // Returns nullptr until the header is successfully read with payload size > 0
    std::uint8_t* body_current_data() noexcept {
        return packet_.get_body_data() ? packet_.get_body_data() + body_already_read_ : nullptr;
    }

    // Returns how much header data is left to read.
    std::size_t head_size_left() const noexcept { return sizeof(PacketHead) - head_already_read_; }

    // Returns how much payload data is left to read.
    std::size_t body_size_left() const noexcept {
        return packet_.get_body_size() - body_already_read_;
    }

    // Extract packet by moving data
    NetPacket extract_packet() noexcept;

    // Return true if all header data was read.
    bool update_head_size(std::size_t size);

    // Return true if all payload data was read.
    bool update_body_size(std::size_t size);

  private:
    std::size_t head_already_read_{};
    std::size_t body_already_read_{};
    NetPacket packet_;
};
}  // namespace lit::net
