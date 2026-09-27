#pragma once

namespace lit::game {
// Fixed-timestep accumulator: add `frame` seconds, clamp to `max_accum` (avoid a
// spiral of death after a stall), and return how many `fixed_dt` steps to run now.
inline int fixed_steps(double& accumulator, double frame, double fixed_dt, double max_accum) {
    accumulator += frame;
    if (accumulator > max_accum) accumulator = max_accum;
    int steps = 0;
    while (accumulator >= fixed_dt) {
        accumulator -= fixed_dt;
        ++steps;
    }
    return steps;
}
}  // namespace lit::game
