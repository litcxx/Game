#pragma once

#include "protocol/server_packet.hpp"
#include "utils/ts_queue.hpp"

namespace lit {
class GameSubsystem {
  public:
    GameSubsystem() = default;
    ~GameSubsystem() = default;
    GameSubsystem(const GameSubsystem&) = delete;
    GameSubsystem& operator=(const GameSubsystem&) = delete;

    TSQueue<std::unique_ptr<ServerPacket>> in_queue;
    TSQueue<std::unique_ptr<ServerPacket>> out_queue;
};
}  // namespace lit
