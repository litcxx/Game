#pragma once

#include "utils/ts_queue.hpp"
#include "protocol/events.hpp"
#include "protocol/base_packet.hpp"

namespace ep::game
{
  class GameSubsystem {
  public:
    ep::TSQueue<net::NetPacket> in_queue_;
    ep::TSQueue<net::NetPacket> out_queue_;
    ep::TSQueue<Event> event_queue_;
  };
}
