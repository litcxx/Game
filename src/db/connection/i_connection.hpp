#pragma once

namespace lit::db {
class IConnection {
  public:
    virtual ~IConnection() = default;
};
}  // namespace lit::db
