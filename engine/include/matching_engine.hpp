#pragma once

#include "order.hpp"
#include "order_book.hpp"
#include <cstdint>
#include <unordered_map>
#include <deque>

// High-level matching engine interface.
// Single-threaded: OrderBook and MatchingEngine share one thread.
class MatchingEngine {
public:
    MatchingEngine() : next_order_id_(1), total_trades_(0) {}

    // Submit an order. Returns SubmitResult with status + any trades.
    // visible_qty: for Iceberg orders, the displayed portion. 0 = full visibility.
    SubmitResult submitOrder(Side side, OrderType type, Price price, Quantity qty,
                             Quantity visible_qty = 0);
    bool cancelOrder(OrderId id);
    const OrderBook& book() const { return book_; }

    uint64_t totalTrades() const { return total_trades_; }

    // ponytail: small rolling trade buffer for VPIN consumption.
    // Add when a more formal event system exists.
    static constexpr size_t MAX_RECENT_TRADES = 256;
    const std::deque<Trade>& recentTrades() const { return recent_trades_; }
    void clearRecentTrades() { recent_trades_.clear(); }

private:
    OrderBook book_;
    OrderId next_order_id_;
    uint64_t total_trades_;
    std::deque<Trade> recent_trades_;

    void recordTrades(const std::vector<Trade>& trades);
};
