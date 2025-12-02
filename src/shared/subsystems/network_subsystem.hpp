#pragma once

#include "utils/ts_queue.hpp"
#include "protocol/base_packet.hpp"
#include "protocol/events.hpp"

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
    TSQueue<Event> event_queue_;
  };
}
