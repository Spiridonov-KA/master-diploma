#include "thread_pool/work_stealing_pool.hpp"

thread_local std::size_t WorkStealingPool::current_thread_index_ = 0;
thread_local bool WorkStealingPool::is_pool_thread_ = false;

WorkStealingPool::WorkStealingPool(std::size_t thread_count) : stop_(false) {
    if (thread_count == 0) {
        thread_count = 1;
    }

    queues_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        queues_.emplace_back(std::make_unique<WorkStealingDeque>());
    }

    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&WorkStealingPool::worker_thread, this, i);
    }
}

WorkStealingPool::~WorkStealingPool() {
    stop_.store(true);
    cv_.notify_all();

    for (std::thread &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::size_t WorkStealingPool::thread_count() const noexcept {
    return workers_.size();
}

bool WorkStealingPool::any_queue_has_tasks() const {
    for (const auto &queue : queues_) {
        if (!queue->empty()) {
            return true;
        }
    }
    return false;
}

void WorkStealingPool::worker_thread(std::size_t index) {
    current_thread_index_ = index;
    is_pool_thread_ = true;

    std::mt19937 rng(index);
    std::uniform_int_distribution<std::size_t> dist(0, workers_.size() - 1);

    static constexpr std::size_t kMaxStealAttempts = 32;

    while (true) {
        std::function<void()> task;

        if (queues_[index]->pop(task)) {
            task();
            continue;
        }

        bool stolen = false;

        for (std::size_t attempt = 0; attempt < kMaxStealAttempts && !stolen;
             ++attempt) {
            std::size_t victim = dist(rng);

            if (victim != index) {
                stolen = try_steal(victim, task);
            }
        }

        if (stolen) {
            task();
            continue;
        }

        {
            std::unique_lock<std::mutex> lock(global_mutex_);

            cv_.wait_for(lock, std::chrono::milliseconds(1), [this] {
                return stop_.load() || any_queue_has_tasks();
            });
        }

        if (stop_.load()) {
            while (queues_[index]->pop(task)) {
                task();
            }
            return;
        }
    }
}

bool WorkStealingPool::try_steal(std::size_t victim_index,
                                 std::function<void()> &task) {
    return queues_[victim_index]->steal(task);
}

std::size_t WorkStealingPool::get_current_thread_index() const {
    if (is_pool_thread_) {
        return current_thread_index_;
    }
    return std::numeric_limits<std::size_t>::max();
}