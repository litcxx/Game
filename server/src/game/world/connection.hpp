#pragma once

#include <cstdint>
#include <optional>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"

namespace lit::game {
// LimitsConfig in ticks, derived once from the tick rate.
struct TickLimits {
    std::uint32_t handshake_ticks;      // no Hello within this: HANDSHAKE_TIMEOUT
    std::uint32_t idle_ticks;           // nothing heard within this: IDLE_TIMEOUT
    std::uint32_t messages_per_second;  // a burst of this many is fine
    std::uint32_t tick_rate;            // ... then it refills at messages_per_second

    static TickLimits from(const LimitsConfig& limits, std::uint32_t tick_rate);
};

// One connection as the World sees it, from Connected to Disconnected: when it
// came and was last heard from, and its message budget — a token bucket kept in
// 1/tick_rate-of-a-message units, so it refills by whole units every tick.
struct Connection {
    std::uint32_t connected_tick{0};
    std::uint32_t heard_tick{0};
    std::uint64_t budget{0};
    std::uint32_t budget_tick{0};  // the tick it was last refilled on
};

// A connection opened on `tick`, with a full budget.
Connection open_connection(std::uint32_t tick, const TickLimits& limits);

// A message from it arrived on `tick`: it is heard from; returns whether its
// budget allows the message (which spends one) — false means RATE_LIMITED.
bool take_message(Connection& connection, std::uint32_t tick, const TickLimits& limits);

// Whether it has run out of time on `tick`: no Hello (`joined` false) within the
// handshake timeout, or nothing heard within the idle timeout.
std::optional<::game::v1::ErrorCode> overdue(const Connection& connection, bool joined,
                                             std::uint32_t tick, const TickLimits& limits);
}  // namespace lit::game
