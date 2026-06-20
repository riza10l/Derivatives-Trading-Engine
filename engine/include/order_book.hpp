#pragma once

#include "order.hpp"
#include "price_level.hpp"
#include <map>
#include <unordered_map>
#include <optional>

// Comparator for price levels:
// - Bids: descending (highest price first) => price larger = "better"
// - Asks: ascending (lowest price first) => price smaller = "better"
struct BidComparator {
    bool operator()(Price a, Price b) const { return a > b; }
};

struct AskComparator {
    bool operator()(Price a, Price b) const { return a < b; }
};

class OrderBook {
public:
    // Add limit order to book
    // Returns: (trades made, quantity added to book)
    // taker_order is consumed: its remaining_qty is updated
    std::vector<Trade> addOrder(Order& taker_order);

    // Add order to book without matching (for PostOnly orders that don't cross)
    bool addToBook(const Order& order);

    // Cancel an order by ID
    bool cancelOrder(OrderId id);

    // Get best bid/ask prices
    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;

    // Get spread (ask - bid)
    std::optional<Price> spread() const;

    // Get depth at price level
    Quantity depthAtPrice(Price price, Side side) const;

    // Total quantity on one side
    Quantity totalBidQty() const;
    Quantity totalAskQty() const;

    // Order book imbalance: (bid_qty - ask_qty) / (bid_qty + ask_qty)
    // Range [-1, 1]: positive = more bids, negative = more asks
    double imbalance() const;

    // For debugging: dump book state
    void dump() const;

private:
    std::map<Price, PriceLevel, BidComparator>  bids_;
    std::map<Price, PriceLevel, AskComparator> asks_;
    std::unordered_map<OrderId, std::pair<Side, Price>> orders_index_;  // id -> (side, price)
};
