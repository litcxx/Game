#pragma once

#include "tile/i_box.hpp"

namespace ep::game {
class IPlayer : public IBox {
  public:
    virtual ~IPlayer() = default;

    virtual std::size_t get_id() const noexcept = 0;
    virtual double get_vel_x() const noexcept = 0;
    virtual double get_vel_y() const noexcept = 0;
    virtual void move(double x, double y) = 0;
    virtual void set_vel(double vel_x, double vel_y) noexcept = 0;
    virtual bool on_ground() const noexcept = 0;
    virtual void set_on_ground(bool state) noexcept = 0;
};
}  // namespace ep::game
