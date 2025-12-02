#pragma once

#include "tile/i_box.hpp"

namespace ep::game
{
  class Tile : public IBox {
  public:
    explicit Tile(double x, double y, std::uint8_t width, std::uint8_t height) noexcept :
      x_(x), y_(y), width_(width), height_(height)
    {}

    double GetX() const noexcept override { return x_; }
    double GetY() const noexcept override { return y_; }
    std::uint8_t GetWidth() const noexcept override { return width_; }
    std::uint8_t GetHeight() const noexcept override { return height_; }
  private:
    double x_;
    double y_;
    std::uint8_t width_;
    std::uint8_t height_;
  };

}
