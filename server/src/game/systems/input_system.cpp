#include "systems/input_system.hpp"

#include <cstdint>

namespace lit::game {
void enqueue_frames(InputQueue& queue, const ::game::v1::Input& input, const LimitsConfig& limits) {
    std::uint32_t taken = 0;
    for (const auto& frame : input.frames()) {
        if (taken++ == limits.max_input_frames) break;          // more than an Input may carry
        if (queue.intents.size() >= limits.input_queue) break;  // too far ahead: drop the rest
        if (frame.input_seq() <= queue.last_enqueued_input_seq)
            continue;  // out-of-order / duplicate
        queue.last_enqueued_input_seq = frame.input_seq();
        queue.intents.push_back(Intent{frame.input_seq(), frame.move_x(), frame.move_y(),
                                       frame.capturing(), frame.attack(), frame.ability_id(),
                                       frame.aim_x(), frame.aim_y()});
    }
}

void consume_inputs(WorldState& state) {
    for (auto& [session_id, session] : state.sessions) {
        InputQueue& queue = session.input;
        if (queue.intents.empty()) {
            // No new frame this tick: the last intent repeats, the same intent under
            // its input_seq — but its attack was used on its own tick, and not again.
            if (Unit* body = find_unit(state, session.character_id)) body->intent.attack = false;
            continue;
        }
        const Intent intent = queue.intents.front();
        queue.intents.pop_front();
        queue.last_input_seq = intent.input_seq;
        if (Unit* body = find_unit(state, session.character_id)) body->intent = intent;
    }
}
}  // namespace lit::game
