#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <new>
#include <utility>

// ponytail: SPSC lock-free ring buffer (LMAX Disruptor pattern)
// Fixed capacity (power of 2), single producer single consumer.
// Cache-line padded: producer and consumer counters never share a cache line.

template <typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    // Producer counter — written by producer, read by consumer
    alignas(64) std::atomic<size_t> write_seq_{0};
    char pad1_[64 - sizeof(std::atomic<size_t>)];

    // Consumer counter — written by consumer, read by producer
    alignas(64) std::atomic<size_t> read_seq_{0};

    static constexpr size_t mask_ = Capacity - 1;
    alignas(alignof(T)) unsigned char storage_[sizeof(T) * Capacity];

    T* slot_(size_t seq) {
        return reinterpret_cast<T*>(storage_ + (seq & mask_) * sizeof(T));
    }

public:
    static constexpr size_t capacity = Capacity;

    ~SPSCRingBuffer() {
        size_t r = read_seq_.load(std::memory_order_relaxed);
        size_t w = write_seq_.load(std::memory_order_relaxed);
        while (r < w) {
            slot_(r)->~T();
            ++r;
        }
    }

    bool try_push(const T& item) {
        size_t w = write_seq_.load(std::memory_order_relaxed);
        size_t r = read_seq_.load(std::memory_order_acquire);
        if (w - r >= Capacity) return false;
        new (slot_(w)) T(item);
        write_seq_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool try_push(T&& item) {
        size_t w = write_seq_.load(std::memory_order_relaxed);
        size_t r = read_seq_.load(std::memory_order_acquire);
        if (w - r >= Capacity) return false;
        new (slot_(w)) T(std::move(item));
        write_seq_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        size_t r = read_seq_.load(std::memory_order_relaxed);
        size_t w = write_seq_.load(std::memory_order_acquire);
        if (r == w) return false;
        T* p = slot_(r);
        item = std::move(*p);
        p->~T();
        read_seq_.store(r + 1, std::memory_order_release);
        return true;
    }

    // Spin until push succeeds (for throughput benchmarks)
    void push(const T& item) {
        for (;;) {
            size_t w = write_seq_.load(std::memory_order_relaxed);
            size_t r = read_seq_.load(std::memory_order_acquire);
            if (w - r < Capacity) {
                new (slot_(w)) T(item);
                write_seq_.store(w + 1, std::memory_order_release);
                return;
            }
            __builtin_ia32_pause();
        }
    }

    // Spin until pop succeeds (for throughput benchmarks)
    T pop() {
        T item;
        for (;;) {
            size_t r = read_seq_.load(std::memory_order_relaxed);
            size_t w = write_seq_.load(std::memory_order_acquire);
            if (r < w) {
                T* p = slot_(r);
                item = std::move(*p);
                p->~T();
                read_seq_.store(r + 1, std::memory_order_release);
                return item;
            }
            __builtin_ia32_pause();
        }
    }

    size_t size() const {
        size_t w = write_seq_.load(std::memory_order_acquire);
        size_t r = read_seq_.load(std::memory_order_acquire);
        return w - r;
    }

    bool empty() const { return size() == 0; }
    bool full() const { return size() >= Capacity; }
};
