#pragma once

#include "utils/ts_queue.hpp"
#include "protocol/net_packet.hpp"

namespace ep
{
  class NetworkSubsystem {
  public:
    NetworkSubsystem() = default;
    ~NetworkSubsystem() = default;
    NetworkSubsystem(const NetworkSubsystem&) = delete;
    NetworkSubsystem& operator=(const NetworkSubsystem&) = delete;

    TSQueue<NetPacket> in_queue_;
    TSQueue<NetPacket> out_queue_;
  };
}
