#pragma once

#include "game/v1/protocol.pb.h"
#include "state/vision.hpp"
#include "state/world_state.hpp"

namespace lit::game {
// The Snapshot for one session under fog of war, given what its character's
// faction sees now (`vision`): `you` (its body's life and timers — NOT_SPAWNED
// while it has none — and the session's input ack); its own body (alive or
// dead) plus every unit and projectile on a visible cell; the visible cells
// changed since the last snapshot plus every cell newly revealed since
// `recipient.sync.vision` (what it was last told), with the revealed / hidden
// deltas; and this period's events whose named units are all listed. `resync`
// (a frame to it was lost): the deltas start from nothing — every visible cell
// is revealed with its state — and Snapshot.resync is set.
::game::v1::ServerMessage build_snapshot(const WorldState& state, const ClientSession& recipient,
                                         const Vision& vision, bool resync);
}  // namespace lit::game
