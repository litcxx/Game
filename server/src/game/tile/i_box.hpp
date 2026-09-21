#pragma once

#include <cstdint>

namespace lit::game {
class IBox {
  public:
    virtual ~IBox() = default;

    virtual double get_x() const noexcept = 0;
    virtual double get_y() const noexcept = 0;
    virtual std::uint8_t get_width() const noexcept = 0;
    virtual std::uint8_t get_height() const noexcept = 0;
};
}  // namespace lit::game
