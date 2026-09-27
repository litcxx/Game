#pragma once

#include <cmath>
#include <optional>

namespace lit::game {
// Swept hit test: a point moving from (x0, y0) to (x1, y1) against a circle of
// radius r at (cx, cy). Returns the fraction t in [0, 1] of the segment at the
// first contact (touching counts), 0 when it starts inside, nullopt on a miss.
inline std::optional<double> segment_circle_hit(double x0, double y0, double x1, double y1,
                                                double cx, double cy, double r) {
    const double fx = x0 - cx;
    const double fy = y0 - cy;
    const double c = fx * fx + fy * fy - r * r;
    if (c <= 0.0) return 0.0;  // already inside (or touching) at the start
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double a = dx * dx + dy * dy;
    if (a == 0.0) return std::nullopt;  // not moving, and outside
    const double b = 2.0 * (fx * dx + fy * dy);
    const double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return std::nullopt;                  // the line misses the circle
    const double t = (-b - std::sqrt(disc)) / (2.0 * a);  // entry point
    if (t < 0.0 || t > 1.0) return std::nullopt;          // behind the start / past the end
    return t;
}
}  // namespace lit::game
