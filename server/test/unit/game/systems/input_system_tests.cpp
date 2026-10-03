#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "config/config.hpp"
#include "game/v1/protocol.pb.h"
#include "state/input_queue.hpp"
#include "state/world_state.hpp"
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

// Session 1 driving the body of character 1.
lit::game::WorldState driven() {
    lit::game::WorldState state;
    state.sessions[1] = lit::game::ClientSession{1, {}, {}};
    lit::game::Unit body;
    body.id = 1;
    state.units[1] = body;
    return state;
}

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

// A tick with no new frame repeats the last intent: the same intent, so under the
// same input_seq. Movement and capture go on; the attack was used on the frame's
// own tick and is not used again, so each attack comes from exactly one frame.
TEST(InputRepeat, AnEmptyQueueRepeatsTheIntentUnderItsInputSeqButNotItsAttack) {
    auto state = driven();
    ::game::v1::Input input;
    auto* frame = input.add_frames();
    frame->set_input_seq(5);
    frame->set_move_x(1);
    frame->set_capturing(true);
    frame->set_attack(true);
    lit::game::enqueue_frames(state.sessions.at(1).input, input, kLimits);
    const lit::game::Intent& intent = state.units.at(1).intent;

    lit::game::consume_inputs(state);  // the frame's tick
    EXPECT_EQ(intent.input_seq, 5u);
    EXPECT_TRUE(intent.attack);

    lit::game::consume_inputs(state);  // no new frame
    EXPECT_EQ(intent.input_seq, 5u);
    EXPECT_EQ(intent.move_x, 1);
    EXPECT_TRUE(intent.capturing);
    EXPECT_FALSE(intent.attack);
    EXPECT_EQ(state.sessions.at(1).input.last_input_seq, 5u);
}
