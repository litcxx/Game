#pragma once

#include <optional>

namespace ep::db 
{
  class IConnection {
  public:
    virtual ~IConnection() = default;
  };
}
