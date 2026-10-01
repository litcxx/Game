#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "config/config.hpp"
#include "state/territory.hpp"
#include "systems/capital_system.hpp"

namespace {

std::uint32_t cell(std::uint32_t col, std::uint32_t row, std::uint32_t width = 100) {
    return row * width + col;
}

bool has(const std::vector<std::uint32_t>& cells, std::uint32_t index) {
    return std::ranges::find(cells, index) != cells.end();
}

}  // namespace

TEST(CapitalZone, IsADiscOfCellsAroundTheCapital) {
    const auto zone = lit::game::capital_zone({1, cell(50, 50), 4}, 100, 100);

    EXPECT_EQ(zone.size(), 49u);  // the cells whose centre is within 4 cells
    EXPECT_TRUE(has(zone, cell(50, 50)));
    EXPECT_TRUE(has(zone, cell(54, 50)));   // 4 to the right
    EXPECT_TRUE(has(zone, cell(50, 46)));   // 4 up
    EXPECT_TRUE(has(zone, cell(52, 53)));   // 4 + 9 <= 16
    EXPECT_FALSE(has(zone, cell(54, 51)));  // 16 + 1 > 16
    EXPECT_FALSE(has(zone, cell(53, 53)));  // 9 + 9 > 16
    EXPECT_FALSE(has(zone, cell(55, 50)));
}

TEST(CapitalZone, IsCutAtTheMapEdge) {
    const auto zone = lit::game::capital_zone({1, cell(0, 0), 4}, 100, 100);

    EXPECT_EQ(zone.size(), 17u);  // a quarter of the disc, its axes included
    EXPECT_TRUE(has(zone, cell(4, 0)));
    EXPECT_TRUE(has(zone, cell(0, 4)));
    EXPECT_TRUE(std::ranges::all_of(zone, [](std::uint32_t i) { return i < 100 * 100; }));
}

TEST(CapitalZone, RadiusZeroIsTheCapitalCellAlone) {
    EXPECT_EQ(lit::game::capital_zone({1, cell(7, 3), 0}, 100, 100),
              std::vector<std::uint32_t>{cell(7, 3)});
}

TEST(SeedCapitals, EachZoneBelongsToItsFaction) {
    lit::game::Territory territory;
    territory.reset(20, 20);
    const std::vector<lit::CapitalConfig> capitals{{1, cell(3, 3, 20), 2},
                                                   {2, cell(15, 15, 20), 2}};

    lit::game::seed_capitals(territory, capitals);

    for (const auto& capital : capitals)
        for (std::uint32_t i : lit::game::capital_zone(capital, 20, 20))
            EXPECT_EQ(territory.owners[i], capital.faction_id) << "cell " << i;
    EXPECT_EQ(std::ranges::count(territory.owners, 1), 13);  // a disc of radius 2
    EXPECT_EQ(std::ranges::count(territory.owners, 2), 13);
    EXPECT_EQ(territory.owned(1), 13u);  // counted as they are (GAME-018)
    EXPECT_EQ(territory.owned(2), 13u);
    EXPECT_EQ(territory.owners[cell(6, 3, 20)], 0u);  // just outside Red's zone
    EXPECT_EQ(territory.owners[cell(10, 10, 20)], 0u);
}

TEST(ProtectCapitals, EveryZoneIsProtectedAndNothingElse) {
    lit::game::Territory territory;
    territory.reset(20, 20);
    const std::vector<lit::CapitalConfig> capitals{{1, cell(3, 3, 20), 2},
                                                   {2, cell(15, 15, 20), 2}};

    lit::game::protect_capitals(territory, capitals);

    for (const auto& capital : capitals)
        for (std::uint32_t i : lit::game::capital_zone(capital, 20, 20))
            EXPECT_TRUE(territory.protected_cells[i]) << "cell " << i;
    EXPECT_EQ(std::ranges::count(territory.protected_cells, true), 26);
    EXPECT_EQ(std::ranges::count(territory.owners, 0), 400);  // the land is the season's
}
