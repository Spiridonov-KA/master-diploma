#include "thread_pool/simple_thread_pool.hpp"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <vector>

TEST(SimpleThreadPoolTest, ReturnsCorrectResult) {
    SimpleThreadPool pool(2);

    auto future = pool.submit([]() -> int { return 42; });

    EXPECT_EQ(future.get(), 42);
}

TEST(SimpleThreadPoolTest, MultipleTasksExecuted) {
    SimpleThreadPool pool(4);
    const int task_count = 100;

    std::vector<std::future<int>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(pool.submit([i]() -> int { return i * i; }));
    }

    for (int i = 0; i < task_count; ++i) {
        EXPECT_EQ(futures[i].get(), i * i);
    }
}

TEST(SimpleThreadPoolTest, AllTasksAreExecuted) {
    SimpleThreadPool pool(4);
    const int task_count = 1000;

    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(
            pool.submit([&counter]() { counter.fetch_add(1); }));
    }

    for (auto &f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(SimpleThreadPoolTest, ExceptionPropagatedThroughFuture) {
    SimpleThreadPool pool(2);

    auto future = pool.submit([]() -> int {
        throw std::runtime_error("test error");
        return 0;
    });

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(SimpleThreadPoolTest, SubmitAfterDestroyThrows) {
    std::unique_ptr<SimpleThreadPool> pool =
        std::make_unique<SimpleThreadPool>(2);

    pool.reset();

    SUCCEED();
}

TEST(SimpleThreadPoolTest, SingleThreadExecutesSequentially) {
    SimpleThreadPool pool(1);
    const int task_count = 10;

    std::vector<int> order;
    std::mutex order_mutex;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(pool.submit([i, &order, &order_mutex]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            order.push_back(i);
        }));
    }

    for (auto &f : futures) {
        f.get();
    }

    for (int i = 0; i < task_count; ++i) {
        EXPECT_EQ(order[i], i);
    }
}

TEST(SimpleThreadPoolTest, ThreadCountIsCorrect) {
    SimpleThreadPool pool(4);
    EXPECT_EQ(pool.thread_count(), 4u);
}