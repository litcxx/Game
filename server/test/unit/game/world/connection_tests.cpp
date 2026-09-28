#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "world/connection.hpp"

namespace {

using lit::game::Connection;
using lit::game::open_connection;
using lit::game::overdue;
using lit::game::take_message;
using lit::game::TickLimits;

// The config.json limits at 60 ticks/s: 120 messages/s is a burst of 120, then
// 2 more each tick; Hello within 300 ticks; silence up to 1200 ticks.
const TickLimits kLimits = TickLimits::from(lit::LimitsConfig{}, 60);

int allowed(Connection& c, std::uint32_t tick, int tries) {
    int n = 0;
    for (int i = 0; i < tries; ++i) n += take_message(c, tick, kLimits) ? 1 : 0;
    return n;
}

}  // namespace

TEST(ConnectionLimits, TicksFromMillisecondsRoundUp) {
    EXPECT_EQ(kLimits.handshake_ticks, 300u);
    EXPECT_EQ(kLimits.idle_ticks, 1200u);
    lit::LimitsConfig tiny;
    tiny.handshake_timeout_ms = 10;  // shorter than a tick still takes one
    EXPECT_EQ(TickLimits::from(tiny, 60).handshake_ticks, 1u);
}

TEST(ConnectionLimits, ABurstUpToTheLimitIsAllowed) {
    Connection c = open_connection(1, kLimits);

    EXPECT_EQ(allowed(c, 1, 120), 120);
    EXPECT_FALSE(take_message(c, 1, kLimits));  // the 121st in the same second
}

TEST(ConnectionLimits, TheBudgetRefillsWithTime) {
    Connection c = open_connection(1, kLimits);
    allowed(c, 1, 120);

    EXPECT_EQ(allowed(c, 2, 5), 2);        // 120/s = 2 a tick
    EXPECT_EQ(allowed(c, 200, 200), 120);  // refilled — but never past a full burst
}

TEST(ConnectionLimits, AMessageEachTickIsNeverLimited) {
    Connection c = open_connection(1, kLimits);

    for (std::uint32_t tick = 1; tick < 3600; ++tick) {
        ASSERT_TRUE(take_message(c, tick, kLimits)) << tick;  // 60/s, a minute long
    }
}

TEST(ConnectionLimits, NoHelloInTimeIsAHandshakeTimeout) {
    const Connection c = open_connection(10, kLimits);

    EXPECT_EQ(overdue(c, /*joined=*/false, 309, kLimits), std::nullopt);
    EXPECT_EQ(overdue(c, /*joined=*/false, 310, kLimits), ::game::v1::ERROR_CODE_HANDSHAKE_TIMEOUT);
    EXPECT_EQ(overdue(c, /*joined=*/true, 310, kLimits), std::nullopt);  // it said Hello
}

TEST(ConnectionLimits, SilenceIsAnIdleTimeout) {
    Connection c = open_connection(10, kLimits);

    EXPECT_EQ(overdue(c, true, 1209, kLimits), std::nullopt);
    EXPECT_EQ(overdue(c, true, 1210, kLimits), ::game::v1::ERROR_CODE_IDLE_TIMEOUT);

    take_message(c, 1000, kLimits);  // any message restarts the clock
    EXPECT_EQ(overdue(c, true, 2199, kLimits), std::nullopt);
    EXPECT_EQ(overdue(c, true, 2200, kLimits), ::game::v1::ERROR_CODE_IDLE_TIMEOUT);
}
