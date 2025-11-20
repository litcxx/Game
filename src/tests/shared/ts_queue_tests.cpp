#include <thread>
#include <utility>

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

TEST(TSQueueTest, PushVal)
{
  ep::TSQueue<int> data;
  std::thread t([&data]{
    data.Push(1);
    data.Push(1);
    data.Push(1);
  });
  t.join();
  
  EXPECT_EQ(data.Size(), 3);
  EXPECT_EQ(data.Empty(), false);
}

TEST(TSQueueTest, PushPtr)
{
  ep::TSQueue<int> data;
  std::thread t([&data]{
    std::shared_ptr<int> a(new int(1));
    data.Push(std::move(a));
  });
  t.join();
  
  EXPECT_EQ(data.Size(), 1);
  EXPECT_EQ(data.Empty(), false);
}

TEST(TSQueueTest, TryPopPtr)
{
  ep::TSQueue<int> data;
  std::shared_ptr<int> value = data.TryPop();
  EXPECT_EQ(value, std::shared_ptr<int>());

  std::thread t([&data]{
    data.Push(1);
  });
  t.join();
  value = data.TryPop();
  EXPECT_EQ(*value, 1);
}

TEST(TSQueueTest, TryPopValue)
{
  ep::TSQueue<int> data;
  int value;
  EXPECT_EQ(data.TryPop(value), false);

  std::thread t([&data]{
    data.Push(1);
  });
  t.join();
  EXPECT_EQ(data.TryPop(value), true);
  EXPECT_EQ(value, 1);
}

TEST(TSQueueTest, WaitAndPopPtr)
{
  ep::TSQueue<int> data;
  std::thread t([&data]{
    std::shared_ptr<int> value = data.WaitAndPop();
    EXPECT_EQ(*value, 1);
  });
  data.Push(1);
  t.join();
  EXPECT_EQ(data.Size(), 0);
}

TEST(TSQueueTest, WaitAndPopValue)
{
  ep::TSQueue<int> data;
  std::thread t([&data]{
    int value;
    data.WaitAndPop(value);
    EXPECT_EQ(value, 1);
  });
  data.Push(1);
  t.join();
  EXPECT_EQ(data.Size(), 0);
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
