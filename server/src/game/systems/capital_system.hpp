#pragma once

#include <cstdint>
#include <vector>

#include "config/config.hpp"
#include "state/territory.hpp"

namespace lit::game {
// A capital's zone (GDD 7.3): the cells whose centre is within `protected_radius`
// cells of the capital cell's centre — a disc, cut at the map edge. Its faction's
// from the start of the season (seed_capitals); its enemies can never take it
// (protect_capitals).
std::vector<std::uint32_t> capital_zone(const CapitalConfig& capital, std::uint32_t width,
                                        std::uint32_t height);

// The start of a season: every capital's zone belongs to its faction.
void seed_capitals(Territory& territory, const std::vector<CapitalConfig>& capitals);

// Every capital's zone is protected from capture (Territory::protected_cells): a
// rule of the config, whatever the season's owners are (they will come from a save).
void protect_capitals(Territory& territory, const std::vector<CapitalConfig>& capitals);
}  // namespace lit::game
