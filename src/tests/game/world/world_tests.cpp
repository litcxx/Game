#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include "protocol/network_subsystem.hpp"
#include "protocol/game_subsystem.hpp"
#include "world/world.hpp"
#include "game/player/mock_player.hpp"

static constexpr uint8_t tick_rate = 60;

TEST(WorldTests, AddPlayers)
{
  auto net_subsystem = std::make_shared<ep::net::NetworkSubsystem>();
  auto game_subsystem = std::make_shared<ep::game::GameSubsystem>();
  ep::game::World world(net_subsystem, game_subsystem, tick_rate);

  EXPECT_EQ(world.PlayerNumbers(), 0);

  for (auto i = 0; i < 5; i++) {
    auto player = std::make_shared<ep::tests::MockPlayer>();

    EXPECT_CALL(*player, GetID())
      .Times(1)
      .WillOnce(testing::Return(i));

    world.AddPlayer(player);
  }

  EXPECT_EQ(world.PlayerNumbers(), 5);
}

TEST(WorldTests, RemovePlayers)
{
  auto net_subsystem = std::make_shared<ep::net::NetworkSubsystem>();
  auto game_subsystem = std::make_shared<ep::game::GameSubsystem>();
  ep::game::World world(net_subsystem, game_subsystem, tick_rate);

  EXPECT_EQ(world.PlayerNumbers(), 0);

  for (auto i = 0; i < 5; i++) {
    auto player = std::make_shared<ep::tests::MockPlayer>();

    EXPECT_CALL(*player, GetID())
      .Times(1)
      .WillOnce(testing::Return(i));

    world.AddPlayer(player);
  }

  EXPECT_EQ(world.PlayerNumbers(), 5);

  for (auto i = 0; i < 5; i++)
    world.RemovePlayer(i);

  EXPECT_EQ(world.PlayerNumbers(), 0);
}
