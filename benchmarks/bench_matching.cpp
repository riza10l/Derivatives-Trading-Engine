#include "matching_engine.hpp"
#include "ring_buffer.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>

using Clock = std::chrono::high_resolution_clock;
using us = std::chrono::microseconds;

template<typename T> inline void DoNotOptimize(const T& v) {
    asm volatile("" : : "m"(v) : "memory");
}

// ---------------------------------------------------------------------------
// Ring buffer throughput — two threads (realistic contention)
// ---------------------------------------------------------------------------
static void bench_ring_buffer_throughput() {
    constexpr size_t N = 10'000'000;
    SPSCRingBuffer<int, 4096> rb;
    std::atomic<size_t> done{0};
    int result = 0;

    auto producer = [&]() {
        for (size_t i = 0; i < N; ++i) {
            rb.push(static_cast<int>(i & 0xFF));
        }
        done.store(1, std::memory_order_release);
    };

    auto consumer = [&]() {
        int last = 0;
        size_t count = 0;
        while (count < N) {
            int val;
            if (rb.try_pop(val)) {
                last = val;
                ++count;
            }
        }
        DoNotOptimize(last);
        result = last;
    };

    auto start = Clock::now();
    std::thread t1(producer), t2(consumer);
    t1.join(); t2.join();
    auto elapsed = std::chrono::duration_cast<us>(Clock::now() - start).count();

    double ops = static_cast<double>(N) * 2.0;
    double sec = elapsed / 1'000'000.0;
    std::cout << "  ring_buffer throughput (2 threads): "
              << std::fixed << std::setprecision(1)
              << ops / sec / 1'000'000.0 << " M ops/sec"
              << "  (" << elapsed / 1000 << " ms)\n";
    DoNotOptimize(result);
}

// ---------------------------------------------------------------------------
// Ring buffer latency (no contention — single threaded)
// ---------------------------------------------------------------------------
static void bench_ring_buffer_latency() {
    constexpr size_t N = 100'000;
    SPSCRingBuffer<int, 4096> rb;
    std::vector<int64_t> samples;
    samples.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        auto start = Clock::now();
        rb.push(42);
        volatile int v = rb.pop();
        (void)v;
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start).count();
        samples.push_back(ns);
    }

    std::sort(samples.begin(), samples.end());
    auto p = [&](double pc) -> int64_t {
        return samples[std::min(N - 1, static_cast<size_t>(N * pc))];
    };

    int64_t p50 = p(0.5);
    std::cout << "  ring_buffer latency: "
              << (p50 ? "p50=" + std::to_string(p50) + "ns" : "p50=<1 μs")
              << "  p99=" << p(0.99) << " ns"
              << "  p99.9=" << p(0.999) << " ns\n";
}

// ---------------------------------------------------------------------------
// Engine throughput: fresh engine per cycle
// ---------------------------------------------------------------------------
static void bench_engine_throughput() {
    constexpr size_t N = 200'000;

    auto start = Clock::now();
    for (size_t i = 0; i < N; ++i) {
        MatchingEngine engine;
        engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
        engine.submitOrder(Side::Ask, OrderType::Limit, 100, 10);
    }
    auto elapsed = std::chrono::duration_cast<us>(Clock::now() - start).count();

    double ops = static_cast<double>(N) * 2.0;
    double sec = elapsed / 1'000'000.0;
    std::cout << "  engine throughput (match cycles): "
              << std::setprecision(1)
              << ops / sec / 1'000'000.0 << " M orders/sec"
              << "  (" << elapsed / 1000 << " ms for " << N << " cycles)\n";
}

// ---------------------------------------------------------------------------
// Engine insert latency: submit limit orders on same engine
// ---------------------------------------------------------------------------
static void bench_engine_latency() {
    constexpr size_t N = 100'000;
    std::vector<int64_t> samples;
    samples.reserve(N);
    MatchingEngine engine;

    for (size_t i = 0; i < N; ++i) {
        auto start = Clock::now();
        engine.submitOrder(Side::Bid, OrderType::Limit, 100 + static_cast<int>(i % 10), 10);
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start).count();
        samples.push_back(ns);
    }

    std::sort(samples.begin(), samples.end());
    auto p = [&](double pc) -> int64_t {
        return samples[std::min(N - 1, static_cast<size_t>(N * pc))];
    };
    std::cout << "  engine insert latency: "
              << "p50=" << p(0.5) << " ns"
              << "  p99=" << p(0.99) << " ns"
              << "  p99.9=" << p(0.999) << " ns\n";
}

// ---------------------------------------------------------------------------
// Full match latency
// ---------------------------------------------------------------------------
static void bench_full_match_latency() {
    constexpr size_t N = 50'000;
    std::vector<int64_t> samples;
    samples.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        MatchingEngine engine;
        engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
        auto start = Clock::now();
        auto result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 10);
        DoNotOptimize(result.trades.size());
        samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start).count());
    }

    std::sort(samples.begin(), samples.end());
    auto p = [&](double pc) -> int64_t {
        return samples[std::min(N - 1, static_cast<size_t>(N * pc))];
    };
    std::cout << "  engine match latency: "
              << "p50=" << p(0.5) << " ns"
              << "  p99=" << p(0.99) << " ns"
              << "  p99.9=" << p(0.999) << " ns\n";
}

// ---------------------------------------------------------------------------
// Imbalance calculation latency
// ---------------------------------------------------------------------------
static void bench_imbalance() {
    constexpr size_t N = 50'000;
    MatchingEngine engine;
    for (int i = 0; i < 100; ++i) {
        engine.submitOrder(Side::Bid, OrderType::Limit, 100 + i, 10);
        engine.submitOrder(Side::Ask, OrderType::Limit, 200 + i, 10);
    }

    std::vector<int64_t> samples;
    samples.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        auto start = Clock::now();
        volatile double imb = engine.book().imbalance();
        (void)imb;
        samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start).count());
    }

    std::sort(samples.begin(), samples.end());
    auto p = [&](double pc) -> int64_t {
        return samples[std::min(N - 1, static_cast<size_t>(N * pc))];
    };
    std::cout << "  imbalance latency: "
              << "p50=" << p(0.5) << " ns"
              << "  p99=" << p(0.99) << " ns\n";
}

int main() {
    std::cout << "\n=== Trading Engine Benchmark v1 ===\n";
    std::cout << "  Machine: MinGW64 | clock ~1 μs | "
              << std::thread::hardware_concurrency() << " threads\n\n";

    std::cout << "--- Ring Buffer (SPSC, Capacity=4096) ---\n";
    bench_ring_buffer_throughput();
    bench_ring_buffer_latency();

    std::cout << "\n--- Matching Engine ---\n";
    bench_engine_throughput();
    bench_engine_latency();
    bench_full_match_latency();
    bench_imbalance();

    std::cout << "\n=== Benchmark v1 complete ===\n";
    return 0;
}

// ponytail: hand-rolled chrono benchmark. Ring buffer uses 2 threads for
// realistic contention. Add Google Benchmark when std::chrono falls short.
