#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

class SimpleThreadPool {
  public:
    explicit SimpleThreadPool(
        std::size_t thread_count = std::thread::hardware_concurrency());

    ~SimpleThreadPool();

    SimpleThreadPool(const SimpleThreadPool &) = delete;
    SimpleThreadPool &operator=(const SimpleThreadPool &) = delete;

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

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);

            if (stop_.load()) {
                throw std::runtime_error(
                    "submit() called on a stopped SimpleThreadPool");
            }

            task_queue_.emplace([task]() { (*task)(); });
        }

        cv_.notify_one();
        return result;
    }

    std::size_t thread_count() const noexcept;

  private:
    void worker_thread();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
};