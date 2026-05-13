#pragma once
#include <array>
#include <atomic>

// Single-producer single-consumer lock-free ring buffer.
// Capacity is N-1 elements (one slot reserved to distinguish full from empty).
template<typename T, size_t N>
class RingBuffer {
public:
    bool push(T val) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t next = advance(h);
        if (next == tail_.load(std::memory_order_acquire))
            return false;  // full
        buf_[h] = val;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& val) {
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire))
            return false;  // empty
        val = buf_[t];
        tail_.store(advance(t), std::memory_order_release);
        return true;
    }

    size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return (h + N - t) % N;
    }

private:
    static size_t advance(size_t i) { return (i + 1) % N; }

    std::array<T, N> buf_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};
