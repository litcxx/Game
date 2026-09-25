#pragma once

#include <cstddef>

namespace lit::game {
class IPlayer {
  public:
    virtual ~IPlayer() = default;

    virtual std::size_t get_id() const noexcept = 0;
    virtual double get_x() const noexcept = 0;
    virtual double get_y() const noexcept = 0;
    virtual void move(double dx, double dy) noexcept = 0;
};
}  // namespace lit::game
