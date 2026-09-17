#include "packet_handler.hpp"

#include <netinet/in.h>
#include <spdlog/spdlog.h>

#include <cstdint>

namespace lit::net {
NetPacket PacketHandler::extract_packet() noexcept {
    head_already_read_ = 0;
    body_already_read_ = 0;
    return std::move(packet_);
}

bool PacketHandler::update_head_size(std::size_t size) {
    // Update already read packet header data.
    head_already_read_ += size;

    spdlog::info("recv: {} bytes", size);

    // Not the entire packet has been read.
    if (head_already_read_ < sizeof(PacketHead)) return false;

    spdlog::info("The full packet header was read");
    spdlog::info("opcode: {}", packet_.get_head_opcode());
    spdlog::info("size: {}", packet_.get_head_size());

    // Checking the validaty of the packet header.
    if (!packet_.is_valid_header()) {
        spdlog::warn("recv invalid header");
        head_already_read_ = 0;
        return false;
    }

    packet_.resize_body(packet_.get_head_size());
    return true;
}

bool PacketHandler::update_body_size(std::size_t size) {
    // Update already read payload data.
    body_already_read_ += size;

    spdlog::info("recv: {} bytes", size);

    // Not all payload data has been read.
    if (body_already_read_ < packet_.get_head_size()) return false;

    spdlog::info("All payload data has been read");

    head_already_read_ = 0;
    body_already_read_ = 0;

    return true;
}
}  // namespace lit::net
