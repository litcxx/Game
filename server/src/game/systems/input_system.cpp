#include "systems/input_system.hpp"

#include <cstdint>

namespace lit::game {
void enqueue_frames(InputQueue& queue, const ::game::v1::Input& input, const LimitsConfig& limits) {
    std::uint32_t taken = 0;
    for (const auto& frame : input.frames()) {
        if (taken++ == limits.max_input_frames) break;           // more than an Input may carry
        if (queue.commands.size() >= limits.input_queue) break;  // too far ahead: drop the rest
        if (frame.seq() <= queue.last_enqueued_seq) continue;    // out-of-order / duplicate
        queue.last_enqueued_seq = frame.seq();
        queue.commands.push_back(InputCommand{
            frame.seq(), Intent{frame.move_x(), frame.move_y(), frame.capturing(), frame.attack(),
                                frame.ability_id(), frame.aim_x(), frame.aim_y()}});
    }
}

void consume_inputs(WorldState& state) {
    for (auto& [session_id, session] : state.sessions) {
        InputQueue& queue = session.input;
        if (queue.commands.empty()) {
            continue;  // no fresh command this tick -> the last intent repeats
        }
        const InputCommand cmd = queue.commands.front();
        queue.commands.pop_front();
        queue.last_input_seq = cmd.seq;
        if (Unit* body = find_unit(state, session.character_id)) body->intent = cmd.intent;
    }
}
}  // namespace lit::game
