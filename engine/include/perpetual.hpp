#pragma once

#include "order.hpp"
#include "order_book.hpp"
#include <cstdint>
#include <optional>

// Simulasi perpetual futures: Mark Price & Funding Rate.
//
// Mark price   = mid price dari order book (best_bid + best_ask)/2
// Funding rate = f(imbalance) di-clamp ke [-max_rate, max_rate]
//
// Pakai OrderBook yang udah existing — no position tracking yet.
// ponytail: single source mid price. Multi-source weighted mark price
// nanti kalau ada real data feed.

struct PerpetualConfig {
    double funding_sensitivity = 1.0;   // scaling: imbalance -> rate
    double max_funding_rate    = 0.001;  // 0.1% per cycle
    double max_premium         = 0.01;   // 1% max diff mark vs index
    Price  tick_size           = 1;      // minimum price increment
};

class PerpetualEngine {
public:
    explicit PerpetualEngine(const OrderBook* book, PerpetualConfig cfg = {});

    // Panggil setiap funding interval (misal tiap N detik simulasi)
    void tick();

    // === Hasil tick terakhir ===
    Price   markPrice()    const { return mark_price_; }
    double  fundingRate()  const { return funding_rate_; }
    Price   lastTradePrice() const { return last_trade_price_; }

    // Update last trade price dari matching engine
    void onTrade(Price trade_price);

    // === Konfigurasi ===
    void setConfig(const PerpetualConfig& cfg) { cfg_ = cfg; }
    const PerpetualConfig& config() const { return cfg_; }

    // === Statistik ===
    uint64_t tickCount() const { return tick_count_; }

private:
    const OrderBook* book_;
    PerpetualConfig cfg_;
    Price   mark_price_;
    Price   last_trade_price_;
    double  funding_rate_;
    uint64_t tick_count_;

    Price computeMidPrice() const;
};
