#pragma once

#include "game/v1/protocol.pb.h"
#include "state/vision.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// The Snapshot for one recipient under fog of war, given what its faction sees
// now (`vision`): `you` (its own life, input ack and timers); the recipient
// itself (alive or a body) plus every spawned player and projectile on a visible
// cell; the visible cells changed since the last snapshot plus every cell newly
// revealed since `recipient.sync.vision` (what it was last told), with the
// revealed / hidden deltas; and this period's events whose named players are all
// listed. `resync` (a frame to it was lost): the deltas start from nothing —
// every visible cell is revealed with its state — and Snapshot.resync is set.
::game::v1::ServerMessage build_snapshot(const WorldState& state, const Player& recipient,
                                         const Vision& vision, bool resync);
}  // namespace lit::game
