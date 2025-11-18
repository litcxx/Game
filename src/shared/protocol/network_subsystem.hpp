#pragma once

#include "utils/ts_queue.hpp"
#include "protocol/base_packet.hpp"

namespace ep::net
{
  class NetworkSubsystem {
  public:
    NetworkSubsystem() = default;
    ~NetworkSubsystem() = default;
    NetworkSubsystem(const NetworkSubsystem&) = delete;
    NetworkSubsystem& operator=(const NetworkSubsystem&) = delete;

    utils::TSQueue<GamePacket> net_in_queue_;
    utils::TSQueue<GamePacket> net_out_queue_;
  };
}
