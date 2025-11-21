#include "packet_handler.hpp"

#include <cstdint>
#include <netinet/in.h>
#include <spdlog/spdlog.h>

namespace ep::net
{
  NetPacket PacketHandler::ExtractPacket() noexcept
  {
    head_already_read_ = 0;
    body_already_read_ = 0;
    return std::move(packet_);
  }

  bool PacketHandler::UpdateHeadSize(std::size_t size)
  {
    // Update already read packet header data.
    head_already_read_ += size;

    spdlog::info("recv: {} bytes", size);

    // Not the entire packet has been read.
    if (head_already_read_ < sizeof(PacketHead))
      return false;

    spdlog::info("The full packet header was read");
    spdlog::info("opcode: {}", packet_.GetHeadOpcode());
    spdlog::info("size: {}", packet_.GetHeadSize());

    // Checking the validaty of the packet header.
    if (!packet_.IsValidHeader()) {
      spdlog::warn("recv invalid header");
      head_already_read_ = 0;
      return false;
    }

    packet_.ResizeBody(packet_.GetHeadSize());
    return true;
  }

  bool PacketHandler::UpdateBodySize(std::size_t size)
  {
    // Update already read payload data.
    body_already_read_ += size;

    spdlog::info("recv: {} bytes", size);

    // Not all payload data has been read.
    if (body_already_read_ < packet_.GetHeadSize())
      return false;

    spdlog::info("All payload data has been read");
  
    head_already_read_ = 0;
    body_already_read_ = 0;

    return true;
  }
}
