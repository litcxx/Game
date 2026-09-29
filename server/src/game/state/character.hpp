#pragma once

#include <cstdint>
#include <string>

namespace lit::game {
// A player's identity in the world, apart from any connection. `id` is the
// public player_id, never reused; its body, once spawned, is the Unit with the
// same id.
struct Character {
    std::uint32_t id{0};
    std::string name;
};
}  // namespace lit::game
