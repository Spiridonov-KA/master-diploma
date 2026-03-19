#include <gtest/gtest.h>
#include "thread_pool/work_stealing_deque.hpp"

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

TEST(WorkStealingDequeTest, InitiallyEmpty)
{
    WorkStealingDeque deque;
    EXPECT_TRUE(deque.empty());
    EXPECT_EQ(deque.size(), 0u);
}

TEST(WorkStealingDequeTest, PushIncreasesSize)
{
    WorkStealingDeque deque;

    deque.push([](){});
    EXPECT_EQ(deque.size(), 1u);

    deque.push([](){});
    EXPECT_EQ(deque.size(), 2u);
}

TEST(WorkStealingDequeTest, PopOnEmptyReturnsFalse)
{
    WorkStealingDeque deque;

    std::function<void()> task;
    EXPECT_FALSE(deque.pop(task));
}

TEST(WorkStealingDequeTest, StealOnEmptyReturnsFalse)
{
    WorkStealingDeque deque;

    std::function<void()> task;
    EXPECT_FALSE(deque.steal(task));
}

TEST(WorkStealingDequeTest, PopIsLIFO)
{
    WorkStealingDeque deque;

    std::vector<int> order;

    deque.push([&order](){ order.push_back(1); });
    deque.push([&order](){ order.push_back(2); });
    deque.push([&order](){ order.push_back(3); });

    std::function<void()> task;

    ASSERT_TRUE(deque.pop(task)); task();
    ASSERT_TRUE(deque.pop(task)); task();
    ASSERT_TRUE(deque.pop(task)); task();

    EXPECT_EQ(order[0], 3);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 1);
}

TEST(WorkStealingDequeTest, StealIsFIFO)
{
    WorkStealingDeque deque;

    std::vector<int> order;

    deque.push([&order](){ order.push_back(1); });
    deque.push([&order](){ order.push_back(2); });
    deque.push([&order](){ order.push_back(3); });

    std::function<void()> task;

    ASSERT_TRUE(deque.steal(task)); task();
    ASSERT_TRUE(deque.steal(task)); task();
    ASSERT_TRUE(deque.steal(task)); task();

    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(WorkStealingDequeTest, PopDecreasesSize)
{
    WorkStealingDeque deque;

    deque.push([](){});
    deque.push([](){});

    std::function<void()> task;
    deque.pop(task);

    EXPECT_EQ(deque.size(), 1u);
}

TEST(WorkStealingDequeTest, GrowsWhenFull)
{
    WorkStealingDeque deque;

    const int task_count = 200;
    std::atomic<int> counter(0);

    for (int i = 0; i < task_count; ++i) {
        deque.push([&counter](){ counter.fetch_add(1); });
    }

    EXPECT_EQ(deque.size(), static_cast<std::size_t>(task_count));

    std::function<void()> task;
    while (deque.pop(task)) {
        task();
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingDequeTest, SingleThiefStealsAll)
{
    WorkStealingDeque deque;

    const int task_count = 100;
    std::atomic<int> counter(0);

    for (int i = 0; i < task_count; ++i) {
        deque.push([&counter](){ counter.fetch_add(1); });
    }

    std::thread thief([&deque]() {
        std::function<void()> task;
        while (!deque.empty()) {
            if (deque.steal(task)) {
                task();
            }
        }
    });

    thief.join();

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingDequeTest, OwnerAndThiefConcurrently)
{
    WorkStealingDeque deque;

    const int task_count = 1000;
    std::atomic<int> counter(0);

    for (int i = 0; i < task_count; ++i) {
        deque.push([&counter](){ counter.fetch_add(1); });
    }

    std::atomic<bool> done(false);

    std::thread thief([&deque, &done]() {
        std::function<void()> task;
        while (!done.load()) {
            if (deque.steal(task)) {
                task();
            }
        }
        while (deque.steal(task)) {
            task();
        }
    });

    std::function<void()> task;
    while (deque.pop(task)) {
        task();
    }

    done.store(true);
    thief.join();

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingDequeTest, MultipleThievesConcurrently)
{
    WorkStealingDeque deque;

    const int task_count  = 10000;
    const int thief_count = 4;
    std::atomic<int> counter(0);

    for (int i = 0; i < task_count; ++i) {
        deque.push([&counter](){ counter.fetch_add(1); });
    }

    std::atomic<bool> done(false);

    std::vector<std::thread> thieves;
    thieves.reserve(thief_count);

    for (int t = 0; t < thief_count; ++t) {
        thieves.emplace_back([&deque, &done]() {
            std::function<void()> task;
            while (!done.load()) {
                if (deque.steal(task)) {
                    task();
                }
            }
            while (deque.steal(task)) {
                task();
            }
        });
    }

    std::function<void()> task;
    while (deque.pop(task)) {
        task();
    }

    done.store(true);

    for (std::thread& t : thieves) {
        t.join();
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingDequeTest, SingleElementRaceCondition)
{
    const int iterations = 10000;

    for (int iter = 0; iter < iterations; ++iter) {
        WorkStealingDeque deque;
        std::atomic<int> counter(0);

        deque.push([&counter](){ counter.fetch_add(1); });

        std::thread thief([&deque, &counter]() {
            std::function<void()> task;
            if (deque.steal(task)) {
                task();
            }
        });

        std::function<void()> task;
        if (deque.pop(task)) {
            task();
        }

        thief.join();

        EXPECT_EQ(counter.load(), 1)
            << "Failed at iteration " << iter;
    }
}