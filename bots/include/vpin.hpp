#pragma once

#include "matching_engine.hpp"
#include <vector>
#include <cstdint>
#include <cmath>
#include <deque>

// ponytail: single-header VPIN calculator. No separate .cpp.
// Bucket-based Volume-Synchronized Probability of Informed Trading.
// VPIN = sum|Vbuy - Vsell| / total_volume over N buckets.
//
// Upgrade: use tick-rule + quote-based classification instead of
//          price-vs-mid heuristic when trade direction matters.

struct VPINCalculator {
    // Config
    Quantity bucket_volume = 1000;  // fixed volume per bucket
    size_t   window_buckets = 50;   // rolling window

    // State
    std::deque<double> vpin_history;
    double current_bucket_buy  = 0;
    double current_bucket_sell = 0;
    double current_bucket_vol  = 0;
    Price  last_trade_price    = 0;

    explicit VPINCalculator(Quantity bucket_vol = 1000, size_t window = 50)
        : bucket_volume(bucket_vol), window_buckets(window) {}

    // Feed a completed trade and return current VPIN (or 0 if not enough data).
    // Uses price-vs-mid heuristic for trade classification.
    double feedTrade(const Trade& trade, Price mid_price) {
        // Classify: buy-initiated if trade price >= mid, else sell-initiated
        double signed_vol = static_cast<double>(trade.qty);
        if (trade.price >= mid_price) {
            current_bucket_buy += signed_vol;
        } else {
            current_bucket_sell += signed_vol;
        }
        current_bucket_vol += signed_vol;
        last_trade_price = trade.price;

        // Check if bucket is full
        double result = 0.0;
        while (current_bucket_vol >= static_cast<double>(bucket_volume)) {
            // Finalize this bucket
            double imbalance = std::abs(current_bucket_buy - current_bucket_sell);
            double vpin = imbalance / current_bucket_vol;
            vpin_history.push_back(vpin);

            // Keep window size
            if (vpin_history.size() > window_buckets) {
                vpin_history.pop_front();
            }

            // Reset bucket (carry over excess volume)
            double excess = current_bucket_vol - static_cast<double>(bucket_volume);
            current_bucket_buy  = 0;
            current_bucket_sell = 0;
            current_bucket_vol  = excess;
        }

        if (vpin_history.size() >= 2) {
            double sum = 0;
            for (auto v : vpin_history) sum += v;
            result = sum / static_cast<double>(vpin_history.size());
        }
        return result;
    }

    // Current rolling average VPIN
    double currentVPIN() const {
        if (vpin_history.empty()) return 0.0;
        double sum = 0;
        for (auto v : vpin_history) sum += v;
        return sum / static_cast<double>(vpin_history.size());
    }

    // Toxicity level: 0 = normal, 1 = maximum
    double toxicity() const {
        double v = currentVPIN();
        // VPIN ranges typically 0.1-0.9; normalize to 0-1
        double t = (v - 0.1) / 0.8;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        return t;
    }

    void reset() {
        vpin_history.clear();
        current_bucket_buy = 0;
        current_bucket_sell = 0;
        current_bucket_vol = 0;
        last_trade_price = 0;
    }
};
