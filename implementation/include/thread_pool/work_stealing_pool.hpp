#pragma once

#include "thread_pool/work_stealing_deque.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class WorkStealingPool {
  public:
    explicit WorkStealingPool(
        std::size_t thread_count = std::thread::hardware_concurrency());

    ~WorkStealingPool();

    WorkStealingPool(const WorkStealingPool &) = delete;
    WorkStealingPool &operator=(const WorkStealingPool &) = delete;

    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(
                                         args)...)]() mutable -> return_type {
                return std::apply(std::move(f), std::move(args));
            });

        std::future<return_type> result = task->get_future();

        std::function<void()> wrapper = [task]() { (*task)(); };

        std::size_t index = get_current_thread_index();

        if (index < queues_.size()) {
            queues_[index]->push(std::move(wrapper));
        } else {
            std::size_t min_index = 0;
            std::size_t min_size = queues_[0]->size();

            for (std::size_t i = 1; i < queues_.size(); ++i) {
                std::size_t s = queues_[i]->size();
                if (s < min_size) {
                    min_size = s;
                    min_index = i;
                }
            }

            queues_[min_index]->push(std::move(wrapper));
        }

        cv_.notify_one();

        return result;
    }

    std::size_t thread_count() const noexcept;

  private:
    void worker_thread(std::size_t index);
    bool try_steal(std::size_t victim_index, std::function<void()> &task);
    std::size_t get_current_thread_index() const;
    bool any_queue_has_tasks() const;

    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<WorkStealingDeque>> queues_;

    std::atomic<bool> stop_;
    std::mutex global_mutex_;
    std::condition_variable cv_;

    static thread_local std::size_t current_thread_index_;
    static thread_local bool is_pool_thread_;
};