#include <thread>

#include <gtest/gtest.h>

#include "utils/ts_queue.hpp"

TEST(TSQueueTest, Empty)
{
  ep::TSQueue<int> data;
  EXPECT_EQ(data.Empty(), true);
}

TEST(TSQueueTest, ZeroSize)
{
  ep::TSQueue<int> data;
  EXPECT_EQ(data.Size(), 0);
}

TEST(TSQueueTest, Push)
{
  ep::TSQueue<int> data;
  EXPECT_EQ(data.Empty(), true);

  std::thread t([&data]{
    data.Push(1);
    data.Push(1);
    data.Push(1);
  });
  t.join();
  
  EXPECT_EQ(data.Size(), 3);
  EXPECT_EQ(data.Empty(), false);
}

TEST(TSQueueTest, TryPop)
{
  ep::TSQueue<int> data;

  auto value = data.TryPop();
  EXPECT_EQ(value, std::nullopt);

  data.Push(1);
  auto value2 = data.TryPop();
  EXPECT_EQ(value2, 1);
}

TEST(TSQueueTest, WaitAndPop)
{
  ep::TSQueue<int> data;
  data.Push(1);

  auto value = data.WaitAndPop();
  EXPECT_EQ(value, 1);
}

TEST(TSQueueTest, Swap)
{
  ep::TSQueue<int>data;

  for (int i = 0; i < 5; i++)
    data.Push(1);
  EXPECT_EQ(data.Size(), 5);

  ep::TSQueue<int> tmp;
  EXPECT_EQ(tmp.Size(), 0);

  TSSwap(tmp, data);

  EXPECT_EQ(data.Size(), 0);
  EXPECT_EQ(tmp.Size(), 5);
}
