#pragma once

#include "protocol/server_packet.hpp"
#include "utils/ts_queue.hpp"

namespace ep
{
  class NetworkSubsystem {
  public:
    NetworkSubsystem() = default;
    ~NetworkSubsystem() = default;
    NetworkSubsystem(const NetworkSubsystem&) = delete;
    NetworkSubsystem& operator=(const NetworkSubsystem&) = delete;

    TSQueue<std::unique_ptr<ServerPacket>> in_queue_;
    TSQueue<std::unique_ptr<ServerPacket>> out_queue_;
  };
}
