#pragma once

#include <cstdint>
#include <optional>

#include "state/world_state.hpp"
#include "systems/spawn_system.hpp"

namespace lit::test {
// A spawn point putting every body in `cell`, for a test whose case needs a body
// somewhere in particular (the World's own rule is the faction's capital).
inline game::SpawnPoint spawn_at(std::uint32_t cell) {
    return [cell](const game::WorldState& /*state*/, std::uint32_t /*character_id*/,
                  std::uint32_t /*faction_id*/) -> std::optional<std::uint32_t> { return cell; };
}
}  // namespace lit::test
