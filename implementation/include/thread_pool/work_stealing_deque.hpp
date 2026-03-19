#pragma once

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <vector>

class RingBuffer {
  public:
    explicit RingBuffer(std::size_t capacity)
        : capacity_(capacity), mask_(capacity - 1),
          buffer_(new std::atomic<void *>[capacity]) {
        assert((capacity & (capacity - 1)) == 0);
    }

    std::size_t capacity() const noexcept { return capacity_; }

    void store(std::size_t index, void *value) noexcept {
        buffer_[index & mask_].store(value, std::memory_order_relaxed);
    }

    void *load(std::size_t index) const noexcept {
        return buffer_[index & mask_].load(std::memory_order_relaxed);
    }

    RingBuffer *grow(std::size_t bottom, std::size_t top) const {
        RingBuffer *new_buffer = new RingBuffer(capacity_ * 2);

        for (std::size_t i = top; i < bottom; ++i) {
            new_buffer->store(i, load(i));
        }

        return new_buffer;
    }

  private:
    std::size_t capacity_;
    std::size_t mask_;
    std::unique_ptr<std::atomic<void *>[]> buffer_;
};

class WorkStealingDeque {
  public:
    static constexpr std::size_t kInitialCapacity = 64;

    WorkStealingDeque()
        : top_(0), bottom_(0), buffer_(new RingBuffer(kInitialCapacity)) {}

    ~WorkStealingDeque() {
        std::size_t top = top_.load(std::memory_order_relaxed);
        std::size_t bottom = bottom_.load(std::memory_order_relaxed);

        RingBuffer *buf = buffer_.load(std::memory_order_relaxed);

        for (std::size_t i = top; i < bottom; ++i) {
            void *ptr = buf->load(i);
            auto *task = static_cast<std::function<void()> *>(ptr);
            delete task;
        }

        delete buf;
    }

    WorkStealingDeque(const WorkStealingDeque &) = delete;
    WorkStealingDeque &operator=(const WorkStealingDeque &) = delete;

    std::size_t size() const noexcept {
        std::size_t bottom = bottom_.load(std::memory_order_relaxed);
        std::size_t top = top_.load(std::memory_order_relaxed);
        return bottom > top ? bottom - top : 0;
    }

    bool empty() const noexcept { return size() == 0; }

    void push(std::function<void()> task) {
        std::size_t bottom = bottom_.load(std::memory_order_relaxed);
        std::size_t top = top_.load(std::memory_order_acquire);

        RingBuffer *buf = buffer_.load(std::memory_order_relaxed);

        if (bottom - top >= buf->capacity() - 1) {
            RingBuffer *new_buf = buf->grow(bottom, top);

            garbage_.push_back(std::unique_ptr<RingBuffer>(buf));

            buffer_.store(new_buf, std::memory_order_relaxed);
            buf = new_buf;
        }

        auto *raw_task = new std::function<void()>(std::move(task));
        buf->store(bottom, raw_task);

        std::atomic_thread_fence(std::memory_order_release);

        bottom_.store(bottom + 1, std::memory_order_relaxed);
    }

    bool pop(std::function<void()> &task) {
        std::size_t bottom = bottom_.load(std::memory_order_relaxed);
        std::size_t top = top_.load(std::memory_order_acquire);

        if (bottom == top) {
            return false;
        }

        bottom -= 1;

        RingBuffer *buf = buffer_.load(std::memory_order_relaxed);

        bottom_.store(bottom, std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        top = top_.load(std::memory_order_relaxed);

        if (top <= bottom) {
            void *ptr = buf->load(bottom);

            if (top == bottom) {
                if (!top_.compare_exchange_strong(top, top + 1,
                                                  std::memory_order_seq_cst,
                                                  std::memory_order_relaxed)) {
                    bottom_.store(bottom + 1, std::memory_order_relaxed);
                    return false;
                }
                bottom_.store(bottom + 1, std::memory_order_relaxed);
            }

            auto *raw_task = static_cast<std::function<void()> *>(ptr);
            task = std::move(*raw_task);
            delete raw_task;
            return true;
        }

        bottom_.store(bottom + 1, std::memory_order_relaxed);
        return false;
    }

    bool steal(std::function<void()> &task) {
        std::size_t top = top_.load(std::memory_order_acquire);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        std::size_t bottom = bottom_.load(std::memory_order_acquire);

        if (top < bottom) {
            void *ptr = buffer_.load(std::memory_order_consume)->load(top);

            if (!top_.compare_exchange_strong(top, top + 1,
                                              std::memory_order_seq_cst,
                                              std::memory_order_relaxed)) {
                return false;
            }

            auto *raw_task = static_cast<std::function<void()> *>(ptr);
            task = std::move(*raw_task);
            delete raw_task;
            return true;
        }

        return false;
    }

  private:
    alignas(64) std::atomic<std::size_t> top_;
    alignas(64) std::atomic<std::size_t> bottom_;
    alignas(64) std::atomic<RingBuffer *> buffer_;

    std::vector<std::unique_ptr<RingBuffer>> garbage_;
};