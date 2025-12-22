#pragma once

#include "protocol/server_packet.hpp"
#include "utils/ts_queue.hpp"

namespace ep
{
  class GameSubsystem {
  public:
    GameSubsystem() = default;
    ~GameSubsystem() = default;
    GameSubsystem(const GameSubsystem&) = delete;
    GameSubsystem& operator=(const GameSubsystem&) = delete;

    TSQueue<std::unique_ptr<ServerPacket>> in_queue_;
    TSQueue<std::unique_ptr<ServerPacket>> out_queue_;
  };
}
