#include <gtest/gtest.h>

#include <cstdint>

#include "state/client_sync.hpp"
#include "sync/delivery.hpp"

namespace {

using lit::game::DropVerdict;
using lit::game::note_drop;

constexpr std::uint32_t kWindow = 300;  // ticks: 5 s at 60 Hz

}  // namespace

TEST(Delivery, AFirstDropAsksForAResync) {
    lit::game::ClientSync sync;

    EXPECT_EQ(note_drop(sync, /*tick=*/10, kWindow), DropVerdict::Resync);
    EXPECT_TRUE(sync.resync);
    EXPECT_EQ(sync.drop_tick, 11u);  // TEMP (LTC-22 DoD): a failing test must turn CI red
}

TEST(Delivery, DropsOnOneTickAreOneDrop) {
    lit::game::ClientSync sync;
    note_drop(sync, 10, kWindow);

    // The same full queue refuses the tick's next frame too (e.g. a roster change).
    EXPECT_EQ(note_drop(sync, 10, kWindow), DropVerdict::Resync);
}

TEST(Delivery, AnotherDropWithinTheWindowCloses) {
    lit::game::ClientSync sync;
    note_drop(sync, 10, kWindow);

    EXPECT_EQ(note_drop(sync, 309, kWindow), DropVerdict::Close);
}

TEST(Delivery, ADropAfterTheWindowStartsOver) {
    lit::game::ClientSync sync;
    note_drop(sync, 10, kWindow);
    sync.resync = false;  // the resync went out

    EXPECT_EQ(note_drop(sync, 310, kWindow), DropVerdict::Resync);
    EXPECT_TRUE(sync.resync);
    EXPECT_EQ(sync.drop_tick, 310u);  // the window runs from the new drop
    EXPECT_EQ(note_drop(sync, 400, kWindow), DropVerdict::Close);
}
