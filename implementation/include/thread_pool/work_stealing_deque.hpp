#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>

class WorkStealingDeque {
  public:
    WorkStealingDeque()  = default;
    ~WorkStealingDeque() = default;

    WorkStealingDeque(const WorkStealingDeque&)            = delete;
    WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;

    // Только владелец
    void push(std::function<void()> task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(task));
    }

    // Только владелец — берёт с конца (LIFO)
    bool pop(std::function<void()>& task)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            return false;
        }

        task = std::move(queue_.back());
        queue_.pop_back();
        return true;
    }

    // Вор — берёт с начала (FIFO)
    bool steal(std::function<void()>& task)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            return false;
        }

        task = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

  private:
    std::deque<std::function<void()>> queue_;
    mutable std::mutex                mutex_;
};