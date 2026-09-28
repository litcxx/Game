#include "world/connection.hpp"

#include <algorithm>

namespace lit::game {
namespace {
// Milliseconds as ticks, rounded up: a limit shorter than a tick still spans one.
std::uint32_t to_ticks(std::uint32_t ms, std::uint32_t tick_rate) {
    const std::uint64_t ticks = (std::uint64_t{ms} * tick_rate + 999) / 1000;
    return static_cast<std::uint32_t>(std::max<std::uint64_t>(ticks, 1));
}

std::uint64_t full_budget(const TickLimits& limits) {
    return std::uint64_t{limits.messages_per_second} * limits.tick_rate;
}
}  // namespace

TickLimits TickLimits::from(const LimitsConfig& limits, std::uint32_t tick_rate) {
    return TickLimits{to_ticks(limits.handshake_timeout_ms, tick_rate),
                      to_ticks(limits.idle_timeout_ms, tick_rate), limits.messages_per_second,
                      tick_rate};
}

Connection open_connection(std::uint32_t tick, const TickLimits& limits) {
    return Connection{tick, tick, full_budget(limits), tick};
}

bool take_message(Connection& connection, std::uint32_t tick, const TickLimits& limits) {
    connection.heard_tick = tick;
    // Refill: messages_per_second units a tick; a message costs tick_rate units.
    const std::uint64_t refill =
        std::uint64_t{tick - connection.budget_tick} * limits.messages_per_second;
    connection.budget = std::min(connection.budget + refill, full_budget(limits));
    connection.budget_tick = tick;
    if (connection.budget < limits.tick_rate) return false;
    connection.budget -= limits.tick_rate;
    return true;
}

std::optional<::game::v1::ErrorCode> overdue(const Connection& connection, bool joined,
                                             std::uint32_t tick, const TickLimits& limits) {
    if (!joined && tick - connection.connected_tick >= limits.handshake_ticks) {
        return ::game::v1::ERROR_CODE_HANDSHAKE_TIMEOUT;
    }
    if (tick - connection.heard_tick >= limits.idle_ticks) {
        return ::game::v1::ERROR_CODE_IDLE_TIMEOUT;
    }
    return std::nullopt;
}
}  // namespace lit::game
