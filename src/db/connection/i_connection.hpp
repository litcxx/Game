#pragma once

namespace ep::db {
class IConnection {
  public:
    virtual ~IConnection() = default;
};
}  // namespace ep::db
