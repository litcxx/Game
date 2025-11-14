#include <thread>
#include <functional>

#include <gtest/gtest.h>

#include "ts_queue.hpp"

using namespace ep::shared;

TEST(TSQueue, EmptyTest)
{
  TSQueue<int> data;
  EXPECT_EQ(data.Empty(), true);
}

TEST(TSQueue, ZeroSizeTest)
{
  TSQueue<int> data;
  EXPECT_EQ(data.Size(), 0);
}

TEST(TSQueue, PushValTest)
{
  TSQueue<int> data;
  std::thread t([&data]{
    data.Push(1);
    data.Push(1);
    data.Push(1);
  });
  t.join();
  
  EXPECT_EQ(data.Size(), 3);
  EXPECT_EQ(data.Empty(), false);
}

TEST(TSQueue, PushPtrTest)
{
  TSQueue<int> data;
  std::thread t([&data]{
    std::shared_ptr<int> a(new int(1));
    data.Push(std::move(a));
  });
  t.join();
  
  EXPECT_EQ(data.Size(), 1);
  EXPECT_EQ(data.Empty(), false);
}

TEST(TSQueue, TryPopPtrTest)
{
  TSQueue<int> data;
  std::shared_ptr<int> value = data.TryPop();
  EXPECT_EQ(value, std::shared_ptr<int>());

  std::thread t([&data]{
    data.Push(1);
  });
  t.join();
  value = data.TryPop();
  EXPECT_EQ(*value, 1);
}

TEST(TSQueue, TryPopValueTest)
{
  TSQueue<int> data;
  int value;
  EXPECT_EQ(data.TryPop(value), false);

  std::thread t([&data]{
    data.Push(1);
  });
  t.join();
  EXPECT_EQ(data.TryPop(value), true);
  EXPECT_EQ(value, 1);
}

TEST(TSQueue, WaitAndPopPtrTest)
{
  TSQueue<int> data;
  std::thread t([&data]{
    std::shared_ptr<int> value = data.WaitAndPop();
    EXPECT_EQ(*value, 1);
  });
  data.Push(1);
  t.join();
  EXPECT_EQ(data.Size(), 0);
}

TEST(TSQueue, WaitAndPopValueTest)
{
  TSQueue<int> data;
  std::thread t([&data]{
    int value;
    data.WaitAndPop(value);
    EXPECT_EQ(value, 1);
  });
  data.Push(1);
  t.join();
  EXPECT_EQ(data.Size(), 0);
}
