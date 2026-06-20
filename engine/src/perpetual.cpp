#include "perpetual.hpp"
#include <algorithm>
#include <cmath>

PerpetualEngine::PerpetualEngine(const OrderBook* book, PerpetualConfig cfg)
    : book_(book), cfg_(cfg), mark_price_(0),
      last_trade_price_(0), funding_rate_(0.0), tick_count_(0) {}

void PerpetualEngine::tick() {
    Price mid = computeMidPrice();

    // Fallback: kalau buku kosong, pertahankan mark_price terakhir
    if (mid != 0) {
        mark_price_ = mid;
    } else if (last_trade_price_ != 0) {
        mark_price_ = last_trade_price_;
    }
    // else: mark_price_ tetap 0 — book baru, belum ada data

    // Clamp mark_price terhadap last_trade_price
    if (last_trade_price_ != 0 && mark_price_ != 0) {
        Price diff = std::llabs(mark_price_ - last_trade_price_);
        Price max_diff = static_cast<Price>(last_trade_price_ * cfg_.max_premium);
        if (diff > max_diff) {
            mark_price_ = (mark_price_ > last_trade_price_)
                ? last_trade_price_ + max_diff
                : last_trade_price_ - max_diff;
        }
    }

    // Funding rate dari order book imbalance
    double imbalance = book_->imbalance();  // range [-1, 1]
    double raw_rate = imbalance * cfg_.funding_sensitivity * static_cast<double>(cfg_.tick_size);
    double max_rate = cfg_.max_funding_rate;
    funding_rate_ = std::clamp(raw_rate, -max_rate, max_rate);

    ++tick_count_;
}

void PerpetualEngine::onTrade(Price trade_price) {
    last_trade_price_ = trade_price;
}

Price PerpetualEngine::computeMidPrice() const {
    auto bid = book_->bestBid();
    auto ask = book_->bestAsk();
    if (!bid || !ask) return 0;

    // integer mid price: (bid + ask) / 2
    return (*bid + *ask) / 2;
}
