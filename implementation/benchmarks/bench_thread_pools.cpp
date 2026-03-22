#include "thread_pool/simple_thread_pool.hpp"
#include "thread_pool/work_stealing_pool.hpp"
#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <future>
#include <vector>

static constexpr int kSmallTaskCount = 100000;

static constexpr auto kLongTaskDelay = std::chrono::microseconds(200);

static void BM_SimplePool_ManySmallTasks(benchmark::State &state) {
    const std::size_t thread_count = static_cast<std::size_t>(state.range(0));

    SimpleThreadPool pool(thread_count);

    for (auto _ : state) {
        std::atomic<int> counter(0);
        std::vector<std::future<void>> futures;
        futures.reserve(kSmallTaskCount);

        for (int i = 0; i < kSmallTaskCount; ++i) {
            futures.emplace_back(pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        for (auto &f : futures) {
            f.get();
        }

        if (counter.load() != kSmallTaskCount) {
            state.SkipWithError("Not all tasks completed");
        }
    }

    state.SetItemsProcessed(state.iterations() * kSmallTaskCount);
}

static void BM_WorkStealingPool_ManySmallTasks(benchmark::State &state) {
    const std::size_t thread_count = static_cast<std::size_t>(state.range(0));

    WorkStealingPool pool(thread_count);

    for (auto _ : state) {
        std::atomic<int> counter(0);
        std::vector<std::future<void>> futures;
        futures.reserve(kSmallTaskCount);

        for (int i = 0; i < kSmallTaskCount; ++i) {
            futures.emplace_back(pool.submit([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        for (auto &f : futures) {
            f.get();
        }

        if (counter.load() != kSmallTaskCount) {
            state.SkipWithError("Not all tasks completed");
        }
    }

    state.SetItemsProcessed(state.iterations() * kSmallTaskCount);
}

static void BM_SimplePool_UnevenLoad(benchmark::State &state) {
    const std::size_t thread_count = static_cast<std::size_t>(state.range(0));

    SimpleThreadPool pool(thread_count);

    for (auto _ : state) {
        std::atomic<int> counter(0);
        std::vector<std::future<void>> futures;
        futures.reserve(kSmallTaskCount);

        for (int i = 0; i < kSmallTaskCount; ++i) {
            futures.emplace_back(pool.submit([&counter, i]() {
                if (i % 2 == 0) {
                    std::this_thread::sleep_for(kLongTaskDelay);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        for (auto &f : futures) {
            f.get();
        }

        if (counter.load() != kSmallTaskCount) {
            state.SkipWithError("Not all tasks completed");
        }
    }

    state.SetItemsProcessed(state.iterations() * kSmallTaskCount);
}

static void BM_WorkStealingPool_UnevenLoad(benchmark::State &state) {
    const std::size_t thread_count = static_cast<std::size_t>(state.range(0));

    WorkStealingPool pool(thread_count);

    for (auto _ : state) {
        std::atomic<int> counter(0);
        std::vector<std::future<void>> futures;
        futures.reserve(kSmallTaskCount);

        for (int i = 0; i < kSmallTaskCount; ++i) {
            futures.emplace_back(pool.submit([&counter, i]() {
                if (i % 2 == 0) {
                    std::this_thread::sleep_for(kLongTaskDelay);
                }
                counter.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        for (auto &f : futures) {
            f.get();
        }

        if (counter.load() != kSmallTaskCount) {
            state.SkipWithError("Not all tasks completed");
        }
    }

    state.SetItemsProcessed(state.iterations() * kSmallTaskCount);
}

BENCHMARK(BM_SimplePool_ManySmallTasks)
    ->Name("SimplePool/ManySmallTasks")
    ->RangeMultiplier(2)
    ->Range(1, 16)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK(BM_WorkStealingPool_ManySmallTasks)
    ->Name("WorkStealingPool/ManySmallTasks")
    ->RangeMultiplier(2)
    ->Range(1, 16)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK(BM_SimplePool_UnevenLoad)
    ->Name("SimplePool/UnevenLoad")
    ->RangeMultiplier(2)
    ->Range(1, 16)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK(BM_WorkStealingPool_UnevenLoad)
    ->Name("WorkStealingPool/UnevenLoad")
    ->RangeMultiplier(2)
    ->Range(1, 16)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();