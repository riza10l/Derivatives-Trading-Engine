#pragma once

#include "order.hpp"
#include "order_book.hpp"
#include <cstdint>
#include <unordered_map>

// High-level matching engine interface.
// Single-threaded: OrderBook and MatchingEngine share one thread.
class MatchingEngine {
public:
    MatchingEngine() : next_order_id_(1), total_trades_(0) {}

    std::vector<Trade> submitOrder(Side side, OrderType type, Price price, Quantity qty);
    bool cancelOrder(OrderId id);
    const OrderBook& book() const { return book_; }

    uint64_t totalTrades() const { return total_trades_; }

private:
    OrderBook book_;
    OrderId next_order_id_;
    uint64_t total_trades_;
};
