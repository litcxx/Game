#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/player.hpp"
#include "systems/input_system.hpp"

namespace {

// An Input carrying frames with the sequence numbers first..last.
::game::v1::Input frames(std::uint32_t first, std::uint32_t last) {
    ::game::v1::Input input;
    for (std::uint32_t seq = first; seq <= last; ++seq) input.add_frames()->set_seq(seq);
    return input;
}

std::vector<std::uint32_t> queued(const lit::game::Player& p) {
    std::vector<std::uint32_t> out;
    for (const auto& cmd : p.inputs) out.push_back(cmd.seq);
    return out;
}

std::vector<std::uint32_t> seqs(std::uint32_t first, std::uint32_t last) {
    std::vector<std::uint32_t> out;
    for (std::uint32_t seq = first; seq <= last; ++seq) out.push_back(seq);
    return out;
}

const lit::LimitsConfig kLimits;  // 8 frames an Input, 32 queued

}  // namespace

TEST(InputLimits, TakesAtMostEightFramesFromOneInput) {
    lit::game::Player p;

    lit::game::enqueue_frames(p, frames(1, 10), kLimits);

    EXPECT_EQ(queued(p), seqs(1, 8));  // 9 and 10 are dropped
}

TEST(InputLimits, TheQueueStopsAtItsLimit) {
    lit::game::Player p;

    for (std::uint32_t first = 1; first <= 33; first += 8) {
        lit::game::enqueue_frames(p, frames(first, first + 7), kLimits);  // 5 x 8 = 40
    }

    EXPECT_EQ(queued(p), seqs(1, 32));  // the newer 8 are dropped
    p.inputs.pop_front();
    lit::game::enqueue_frames(p, frames(41, 42), kLimits);
    EXPECT_EQ(p.inputs.back().seq, 41u);  // room for one again
    EXPECT_EQ(p.inputs.size(), 32u);
}

TEST(InputLimits, OldOrRepeatedFramesAreIgnored) {
    lit::game::Player p;
    lit::game::enqueue_frames(p, frames(1, 3), kLimits);

    lit::game::enqueue_frames(p, frames(2, 5), kLimits);

    EXPECT_EQ(queued(p), seqs(1, 5));  // 2 and 3 were already in
}
