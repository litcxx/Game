#pragma once

#include "utils/ts_queue.hpp"
#include "protocol/events.hpp"
#include "protocol/base_packet.hpp"

namespace ep
{
  class GameSubsystem {
  public:
    TSQueue<NetPacket> in_queue_;
    TSQueue<NetPacket> out_queue_;
    TSQueue<Event> event_queue_;
  };
}
