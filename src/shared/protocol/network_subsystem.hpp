#pragma once

#include "utils/ts_queue.hpp"
#include "protocol/base_packet.hpp"
#include "protocol/events.hpp"

namespace ep::net
{
  class NetworkSubsystem {
  public:
    NetworkSubsystem() = default;
    ~NetworkSubsystem() = default;
    NetworkSubsystem(const NetworkSubsystem&) = delete;
    NetworkSubsystem& operator=(const NetworkSubsystem&) = delete;

    ep::TSQueue<NetPacket> in_queue_;
    ep::TSQueue<NetPacket> out_queue_;
    ep::TSQueue<Event> event_queue_;
  };
}
