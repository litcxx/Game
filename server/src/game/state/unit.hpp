#pragma once

#include <cstdint>

#include "game/v1/protocol.pb.h"

namespace lit::game {
// What a unit is trying to do. A player's comes from its input, one command per
// tick (consume_inputs); a monster's will come from its AI.
struct Intent {
    std::int32_t move_x{0};  // movement direction (any length; normalized when moving)
    std::int32_t move_y{0};
    bool capturing{false};        // holding capture: captures the cell under the centre
    bool attack{false};           // holding attack: use the selected ability when ready
    std::uint32_t ability_id{0};  // selected Ability.id (0 = the first)
    std::int32_t aim_x{0};        // aim direction from the centre (projectiles)
    std::int32_t aim_y{0};
};

// A body in the world: it moves, fights and takes damage. A player's has its
// character's id (the public player_id); a monster's will have an id from the
// world entities' range. It exists from its spawn until it leaves the world; a
// killed one stays as a body (DEAD) until it respawns.
struct Unit {
    std::uint32_t id{0};
    std::uint32_t faction_id{0};
    ::game::v1::LifeState life{::game::v1::LIFE_STATE_ALIVE};  // ALIVE or DEAD
    double x{0.0};  // position in units (100 units = 1 cell)
    double y{0.0};
    std::uint32_t hp{0};
    Intent intent;
    std::uint32_t attack_ready_tick{0};     // next tick it may attack (shared cooldown)
    std::uint32_t cooldown_ticks{0};        // length of the current cooldown (last ability used)
    std::uint32_t block_until_tick{0};      // blocking (no damage) while tick < this
    std::uint32_t block_ready_tick{0};      // next tick a block may start (its own cooldown)
    std::uint32_t block_cooldown_ticks{0};  // length of the current block cooldown
    std::uint32_t respawn_tick{0};          // when DEAD: tick from which respawn is allowed
};
}  // namespace lit::game
