#include "packet_handler.hpp"

#include <spdlog/spdlog.h>

namespace ep::net
{
  PacketHandler::PacketHandler() :
    header_read_(0),
    body_read_(0)
  {}

  uint8_t* PacketHandler::HeaderData() noexcept
  {
    return reinterpret_cast<uint8_t*>(&packet_.head_) + header_read_;
  }

  uint8_t* PacketHandler::BodyData() noexcept
  {
    return reinterpret_cast<uint8_t*>(packet_.body_.data()) + body_read_;
  }

  std::size_t PacketHandler::HeaderSize() const noexcept
  {
    return sizeof(PacketHead) - header_read_;
  }

  std::size_t PacketHandler::BodySize() const noexcept
  {
    return packet_.body_.size() - body_read_;
  }

  PacketData PacketHandler::ExtractPacket() noexcept
  {
    header_read_ = 0;
    body_read_ = 0;
    return std::move(packet_);
  }

  bool PacketHandler::ReadHeader(std::size_t size)
  {
    // Update already read packet header data.
    header_read_ += size;

    spdlog::info("recv: {} bytes", size);

    // Not the entire packet has been read.
    if (header_read_ < sizeof(PacketHead))
      return false;

    spdlog::info("The full packet header was read");
    spdlog::info("opcode: {}", packet_.head_.opcode_);
    spdlog::info("size: {}", packet_.head_.size_);

    // Checking the validaty of the packet header.
    if (!packet_.head_.IsValid()) {
      spdlog::warn("recv invalid header");
      // spdlog::warn("recv invalid header from ip: {}",
      //              socket_->string_address());
      // head_bytes_read_ = 0;
      // return server_->CloseSession(shared_from_this());
      header_read_ = 0;
      return false;
    }

    packet_.body_.resize(packet_.head_.size_);
    return true;
  }

  bool PacketHandler::ReadBody(std::size_t size)
  {
    // Update already read payload data.
    body_read_ += size;

    spdlog::info("recv: {} bytes", size);

    // Not all payload data has been read.
    if (body_read_ < packet_.head_.size_)
      return false;

    spdlog::info("All payload data has been read");
  
    header_read_ = 0;
    body_read_ = 0;

    return true;
  }
}
