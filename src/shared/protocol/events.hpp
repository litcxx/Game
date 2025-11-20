#pragma once

#include <cstddef>
#include <cstdint>

namespace ep
{
  enum class EventCode : uint8_t {
    AddNewPlayer        = 0x00,
    RemovePlayer        = 0x01,
  };

  struct Event {
    EventCode code_;
    std::size_t id_;

    explicit Event(EventCode code, std::size_t id) noexcept :
      code_(code),
      id_(id)
    {}
  };
}
