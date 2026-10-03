#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/input_queue.hpp"
#include "systems/input_system.hpp"

namespace {

// An Input carrying frames with the input seqs first..last.
::game::v1::Input frames(std::uint32_t first, std::uint32_t last) {
    ::game::v1::Input input;
    for (std::uint32_t n = first; n <= last; ++n) input.add_frames()->set_input_seq(n);
    return input;
}

std::vector<std::uint32_t> queued(const lit::game::InputQueue& q) {
    std::vector<std::uint32_t> out;
    for (const auto& intent : q.intents) out.push_back(intent.input_seq);
    return out;
}

std::vector<std::uint32_t> input_seqs(std::uint32_t first, std::uint32_t last) {
    std::vector<std::uint32_t> out;
    for (std::uint32_t n = first; n <= last; ++n) out.push_back(n);
    return out;
}

const lit::LimitsConfig kLimits;  // 8 frames an Input, 32 queued

}  // namespace

TEST(InputLimits, TakesAtMostEightFramesFromOneInput) {
    lit::game::InputQueue p;

    lit::game::enqueue_frames(p, frames(1, 10), kLimits);

    EXPECT_EQ(queued(p), input_seqs(1, 8));  // 9 and 10 are dropped
}

TEST(InputLimits, TheQueueStopsAtItsLimit) {
    lit::game::InputQueue p;

    for (std::uint32_t first = 1; first <= 33; first += 8) {
        lit::game::enqueue_frames(p, frames(first, first + 7), kLimits);  // 5 x 8 = 40
    }

    EXPECT_EQ(queued(p), input_seqs(1, 32));  // the newer 8 are dropped
    p.intents.pop_front();
    lit::game::enqueue_frames(p, frames(41, 42), kLimits);
    EXPECT_EQ(p.intents.back().input_seq, 41u);  // room for one again
    EXPECT_EQ(p.intents.size(), 32u);
}

TEST(InputLimits, OldOrRepeatedFramesAreIgnored) {
    lit::game::InputQueue p;
    lit::game::enqueue_frames(p, frames(1, 3), kLimits);

    lit::game::enqueue_frames(p, frames(2, 5), kLimits);

    EXPECT_EQ(queued(p), input_seqs(1, 5));  // 2 and 3 were already in
}
