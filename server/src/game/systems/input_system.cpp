#include "systems/input_system.hpp"

#include <cstdint>

namespace lit::game {
void enqueue_frames(Player& player, const ::game::v1::Input& input, const LimitsConfig& limits) {
    std::uint32_t taken = 0;
    for (const auto& frame : input.frames()) {
        if (taken++ == limits.max_input_frames) break;          // more than an Input may carry
        if (player.inputs.size() >= limits.input_queue) break;  // too far ahead: drop the rest
        if (frame.seq() <= player.last_enqueued_seq) continue;  // out-of-order / duplicate
        player.last_enqueued_seq = frame.seq();
        player.inputs.push_back(InputCommand{frame.seq(), frame.move_x(), frame.move_y(),
                                             frame.capturing(), frame.attack(), frame.ability(),
                                             frame.aim_x(), frame.aim_y()});
    }
}

void consume_inputs(WorldState& state) {
    for (auto& [session_id, p] : state.players) {
        if (p.inputs.empty()) {
            continue;  // no fresh command this tick -> repeat last intent (fields unchanged)
        }
        const InputCommand cmd = p.inputs.front();
        p.inputs.pop_front();
        p.move_x = cmd.move_x;
        p.move_y = cmd.move_y;
        p.capturing = cmd.capturing;
        p.attack = cmd.attack;
        p.ability = cmd.ability;
        p.aim_x = cmd.aim_x;
        p.aim_y = cmd.aim_y;
        p.last_input_seq = cmd.seq;
    }
}
}  // namespace lit::game
