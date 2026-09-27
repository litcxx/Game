#pragma once

#include <cstdint>
#include <deque>
#include <string>

#include "game/v1/protocol.pb.h"

namespace lit::game {
// One tick's worth of player intent, consumed one-per-tick for deterministic replay.
struct InputCommand {
    std::uint32_t seq{0};
    std::int32_t move_x{0};
    std::int32_t move_y{0};
    bool capturing{false};
    bool attack{false};
};

// Per-connection game state. The session_id (its key in WorldState::players) is
// the transport key; `id` is the public player_id (>= 1) used on the wire.
struct Player {
    std::uint32_t id{0};
    std::string name;
    std::uint32_t faction_id{0};  // 0 until spawned
    ::game::v1::LifeState life{::game::v1::LIFE_STATE_NOT_SPAWNED};

    double x{0.0};  // position in units (100 units = 1 cell)
    double y{0.0};
    std::uint32_t hp{0};
    std::uint32_t last_input_seq{0};
    std::int32_t move_x{0};  // current intent, set from the consumed InputCommand
    std::int32_t move_y{0};
    bool capturing{false};  // holding the capture key: captures the cell under the center
    bool attack{false};     // holding the attack key: area hit around the player when ready
    std::uint32_t attack_ready_tick{0};  // next tick this player may attack
    std::uint32_t respawn_tick{0};       // when DEAD: tick from which respawn is allowed

    std::deque<InputCommand> inputs;     // pending per-tick commands (FIFO by seq)
    std::uint32_t last_enqueued_seq{0};  // highest seq accepted into the queue
};
}  // namespace lit::game
