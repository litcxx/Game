#include <gtest/gtest.h>

#include <thread>

#include "utils/ts_queue.hpp"

TEST(TSQueueTest, Empty) {
    lit::TSQueue<int> data;
    EXPECT_EQ(data.empty(), true);
}

TEST(TSQueueTest, ZeroSize) {
    lit::TSQueue<int> data;
    EXPECT_EQ(data.size(), 0);
}

TEST(TSQueueTest, Push) {
    lit::TSQueue<int> data;
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
    lit::TSQueue<int> data;

    auto value = data.try_pop();
    EXPECT_EQ(value, std::nullopt);

    data.push(1);
    auto value2 = data.try_pop();
    EXPECT_EQ(value2, 1);
}

TEST(TSQueueTest, WaitAndPop) {
    lit::TSQueue<int> data;
    data.push(1);

    auto value = data.wait_and_pop();
    EXPECT_EQ(value, 1);
}

TEST(TSQueueTest, Swap) {
    lit::TSQueue<int> data;

    for (int i = 0; i < 5; i++) data.push(1);
    EXPECT_EQ(data.size(), 5);

    lit::TSQueue<int> tmp;
    EXPECT_EQ(tmp.size(), 0);

    swap(tmp, data);

    EXPECT_EQ(data.size(), 0);
    EXPECT_EQ(tmp.size(), 5);
}
