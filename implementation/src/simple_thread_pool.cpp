#include "thread_pool/simple_thread_pool.hpp"

SimpleThreadPool::SimpleThreadPool(std::size_t thread_count) : stop_(false) {
    if (thread_count == 0) {
        thread_count = 1;
    }

    workers_.reserve(thread_count);

    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&SimpleThreadPool::worker_thread, this);
    }
}

SimpleThreadPool::~SimpleThreadPool() {
    stop_.store(true);

    cv_.notify_all();

    for (std::thread &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::size_t SimpleThreadPool::thread_count() const noexcept {
    return workers_.size();
}

void SimpleThreadPool::worker_thread() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            cv_.wait(lock,
                     [this] { return stop_.load() || !task_queue_.empty(); });

            if (stop_.load() && task_queue_.empty()) {
                return;
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        task();
    }
}