#pragma once

#include <cstdint>
#include <deque>
#include <string>

#include "game/v1/protocol.pb.h"
#include "state/vision.hpp"

namespace lit::game {
// One tick's worth of player intent, consumed one-per-tick for deterministic replay.
struct InputCommand {
    std::uint32_t seq{0};
    std::int32_t move_x{0};
    std::int32_t move_y{0};
    bool capturing{false};
    bool attack{false};
    std::uint32_t ability{0};  // Ability.id to use; 0 = the first (default) ability
    std::int32_t aim_x{0};     // aim direction from the player's centre (any length)
    std::int32_t aim_y{0};
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
    bool capturing{false};     // holding the capture key: captures the cell under the center
    bool attack{false};        // holding the attack key: use the selected ability when ready
    std::uint32_t ability{0};  // selected Ability.id (0 = the first)
    std::int32_t aim_x{0};     // aim direction (projectiles)
    std::int32_t aim_y{0};
    std::uint32_t attack_ready_tick{0};     // next tick this player may attack (shared cooldown)
    std::uint32_t cooldown_ticks{0};        // length of the current cooldown (last ability used)
    std::uint32_t block_until_tick{0};      // blocking (no damage) while tick < this
    std::uint32_t block_ready_tick{0};      // next tick a block may start (its own cooldown)
    std::uint32_t block_cooldown_ticks{0};  // length of the current block cooldown
    std::uint32_t respawn_tick{0};          // when DEAD: tick from which respawn is allowed

    std::deque<InputCommand> inputs;     // pending per-tick commands (FIFO by seq)
    std::uint32_t last_enqueued_seq{0};  // highest seq accepted into the queue

    // Fog of war: the cells this client was last told it sees — the base for the
    // next snapshot's revealed / hidden deltas. Kept across death and respawn.
    Vision vision;
};
}  // namespace lit::game
