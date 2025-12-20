#pragma once

#include "utils/ts_queue.hpp"
#include "protocol/base_packet.hpp"

namespace ep
{
  class GameSubsystem {
  public:
    GameSubsystem() = default;
    ~GameSubsystem() = default;
    GameSubsystem(const GameSubsystem&) = delete;
    GameSubsystem& operator=(const GameSubsystem&) = delete;

    TSQueue<NetPacket> in_queue_;
    TSQueue<NetPacket> out_queue_;
  };
}
