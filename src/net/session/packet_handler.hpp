#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol/net_packet.hpp"

namespace ep::net
{
  class PacketHandler {
  public:
    PacketHandler() = default;
    ~PacketHandler() = default;
    PacketHandler(const PacketHandler&) = delete;
    PacketHandler& operator=(const PacketHandler&) = delete;

    // Get packet handler data.
    std::size_t HeadAlreadyRead() const noexcept { return head_already_read_; }
    std::size_t BodyAlreadyRead() const noexcept { return body_already_read_; }

    // Return pointer to current position in header buffer.
    std::uint8_t* HeadCurrentData() noexcept { return packet_.GetHeadData() + head_already_read_; }

    // Return pointer to current position in body buffer.
    // Returns nullptr until the header is successfully read with payload size > 0
    std::uint8_t* BodyCurrentData() noexcept { return packet_.GetBodyData() ? packet_.GetBodyData() + body_already_read_ : nullptr; }

    // Returns how much header data is left to read.
    std::size_t HeadSizeLeft() const noexcept { return sizeof(PacketHead) - head_already_read_; }

    // Returns how much payload data is left to read.
    std::size_t BodySizeLeft() const noexcept { return packet_.GetBodySize() - body_already_read_; }

    // Extract packet by moving data
    NetPacket ExtractPacket() noexcept;

    // Return true if all header data was read.
    bool UpdateHeadSize(std::size_t size);

    // Return true if all payload data was read.
    bool UpdateBodySize(std::size_t size);

  private:
    std::size_t head_already_read_{};
    std::size_t body_already_read_{};
    NetPacket packet_;
  };
}
