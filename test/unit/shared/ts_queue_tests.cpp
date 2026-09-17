#include <gtest/gtest.h>

#include <thread>

#include "utils/ts_queue.hpp"

TEST(TSQueueTest, Empty) {
    ep::TSQueue<int> data;
    EXPECT_EQ(data.empty(), true);
}

TEST(TSQueueTest, ZeroSize) {
    ep::TSQueue<int> data;
    EXPECT_EQ(data.size(), 0);
}

TEST(TSQueueTest, Push) {
    ep::TSQueue<int> data;
    EXPECT_EQ(data.empty(), true);

    std::thread t([&data] {
        data.push(1);
        data.push(1);
        data.push(1);
    });
    t.join();

    EXPECT_EQ(data.size(), 3);
    EXPECT_EQ(data.empty(), false);
}

TEST(TSQueueTest, TryPop) {
    ep::TSQueue<int> data;

    auto value = data.try_pop();
    EXPECT_EQ(value, std::nullopt);

    data.push(1);
    auto value2 = data.try_pop();
    EXPECT_EQ(value2, 1);
}

TEST(TSQueueTest, WaitAndPop) {
    ep::TSQueue<int> data;
    data.push(1);

    auto value = data.wait_and_pop();
    EXPECT_EQ(value, 1);
}

TEST(TSQueueTest, Swap) {
    ep::TSQueue<int> data;

    for (int i = 0; i < 5; i++) data.push(1);
    EXPECT_EQ(data.size(), 5);

    ep::TSQueue<int> tmp;
    EXPECT_EQ(tmp.size(), 0);

    ts_swap(tmp, data);

    EXPECT_EQ(data.size(), 0);
    EXPECT_EQ(tmp.size(), 5);
}
