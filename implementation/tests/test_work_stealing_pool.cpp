#include <gtest/gtest.h>
#include "thread_pool/work_stealing_pool.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <vector>

TEST(WorkStealingPoolTest, ReturnsCorrectResult)
{
    WorkStealingPool pool(2);

    auto future = pool.submit([]() -> int { return 42; });

    EXPECT_EQ(future.get(), 42);
}

TEST(WorkStealingPoolTest, MultipleTasksExecuted)
{
    WorkStealingPool pool(4);
    const int task_count = 100;

    std::vector<std::future<int>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(
            pool.submit([i]() -> int { return i * i; }));
    }

    for (int i = 0; i < task_count; ++i) {
        EXPECT_EQ(futures[i].get(), i * i);
    }
}

TEST(WorkStealingPoolTest, AllTasksAreExecuted)
{
    int x = 8;
    WorkStealingPool pool(4);
    const int task_count = 1000;

    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(
            pool.submit([&counter]() {
                counter.fetch_add(1);
            }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingPoolTest, ExceptionPropagatedThroughFuture)
{
    WorkStealingPool pool(2);

    auto future = pool.submit([]() -> int {
        throw std::runtime_error("test error");
        return 0;
    });

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(WorkStealingPoolTest, ThreadCountIsCorrect)
{
    WorkStealingPool pool(4);
    EXPECT_EQ(pool.thread_count(), 4u);
}

TEST(WorkStealingPoolTest, SingleThreadExecutesAllTasks)
{
    WorkStealingPool pool(1);
    const int task_count = 100;

    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(
            pool.submit([&counter]() {
                counter.fetch_add(1);
            }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingPoolTest, HeavyLoadAllTasksExecuted)
{
    WorkStealingPool pool(8);
    const int task_count = 100000;

    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(
            pool.submit([&counter]() {
                counter.fetch_add(1);
            }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingPoolTest, UnevenTaskDurations)
{
    WorkStealingPool pool(4);
    const int task_count = 20;

    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(
            pool.submit([i, &counter]() {
                if (i % 2 == 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(10));
                }
                counter.fetch_add(1);
            }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(WorkStealingPoolTest, TasksSubmitSubtasksWithoutWaiting)
{
    WorkStealingPool pool(4);

    std::atomic<int> counter(0);

    const int outer_count = 10;
    const int inner_count = 10;

    std::vector<std::promise<void>> promises(outer_count * inner_count);
    std::vector<std::future<void>>  futures;
    futures.reserve(outer_count * inner_count);

    for (auto& p : promises) {
        futures.emplace_back(p.get_future());
    }

    std::atomic<int> promise_index(0);

    for (int i = 0; i < outer_count; ++i) {
        pool.submit([&pool, &counter, &promises,
                     &promise_index, inner_count]() {
            for (int j = 0; j < inner_count; ++j) {
                int idx = promise_index.fetch_add(1);
                pool.submit([&counter, &promises, idx]() {
                    counter.fetch_add(1);
                    promises[idx].set_value();
                });
            }
        });
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), outer_count * inner_count);
}

TEST(WorkStealingPoolTest, ResultsAreCorrectUnderContention)
{
    WorkStealingPool pool(8);
    const int task_count = 1000;

    std::vector<std::future<int>> futures;
    futures.reserve(task_count);

    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(
            pool.submit([i]() -> int {
                int result = 0;
                for (int j = 0; j <= i % 100; ++j) {
                    result += j;
                }
                return result;
            }));
    }

    for (int i = 0; i < task_count; ++i) {
        int expected = 0;
        for (int j = 0; j <= i % 100; ++j) {
            expected += j;
        }
        EXPECT_EQ(futures[i].get(), expected);
    }
}

TEST(WorkStealingPoolTest, DestructorWaitsForPendingTasks)
{
    std::atomic<int> counter(0);

    {
        WorkStealingPool pool(4);

        for (int i = 0; i < 100; ++i) {
            pool.submit([&counter]() {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1));
                counter.fetch_add(1);
            });
        }

    }

    EXPECT_EQ(counter.load(), 100);
}