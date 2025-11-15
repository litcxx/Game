#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol/base_packet.hpp"

namespace ep::net
{
  class PacketHandler {
  public:
    PacketHandler();
    PacketHandler(const PacketHandler&) = delete;
    PacketHandler& operator=(const PacketHandler&) = delete;
    ~PacketHandler() = default;

    // Return pointer to current position in header buffer.
    uint8_t* HeaderData() noexcept;

    // Return pointer to current position in body buffer.
    // Returns nullptr until the header is successfully read with payload size > 0
    uint8_t* BodyData() noexcept;

    // Returns how much header data is left to read.
    std::size_t HeaderSize() const noexcept;

    // Returns how much payload data is left to read.
    std::size_t BodySize() const noexcept;

    // Extract packet by moving data
    PacketData ExtractPacket() noexcept;

    // Return true if all header data was read.
    bool ReadHeader(std::size_t size);

    // Return true if all payload data was read.
    bool ReadBody(std::size_t size);

    std::size_t GetHeadRead() const noexcept { return header_read_; }
    std::size_t GetBodyRead() const noexcept { return body_read_; }

  private:
    std::size_t header_read_;
    std::size_t body_read_;
    PacketData packet_;
  };
}
