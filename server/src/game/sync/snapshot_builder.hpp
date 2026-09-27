#pragma once

#include "game/v1/protocol.pb.h"
#include "state/world_state.hpp"

namespace lit::game {
// The Snapshot for one recipient: `you` (its own life, input ack and timers),
// every spawned player (alive or a body), the cells changed since the last
// snapshot and this period's events. Built per recipient — the seam for interest
// management / fog of war.
::game::v1::ServerMessage build_snapshot(const WorldState& state, const Player& recipient);
}  // namespace lit::game
