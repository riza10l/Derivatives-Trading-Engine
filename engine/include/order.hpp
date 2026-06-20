#pragma once

#include <cstdint>
#include <chrono>
#include <vector>

// Order side: buy or sell
enum class Side { Bid, Ask };

// Order type
enum class OrderType { Limit, Market, PostOnly, FOK, IOC, Iceberg };

// Submit result status
enum class SubmitStatus {
    Accepted,       // order resting in book (may have partial fills)
    PartialFill,    // IOC: partially filled, remainder cancelled
    Filled,         // fully filled
    Rejected,       // post-only crossed, or other rejection
    Cancelled       // FOK couldn't fill full qty
};

// ponytail: OrderStatus enum dropped — status deduced from remaining_qty
// Ponytail: TradeCallback dropped — YAGNI, add when an external
// listener is needed.

using OrderId = uint64_t;
using Price = int64_t;          // price in ticks (e.g., cents for BTC/USD)
using Quantity = uint32_t;
using Timestamp = std::chrono::microseconds;

// Cache-aligned order struct
struct alignas(64) Order {
    OrderId      id;
    Side         side;
    OrderType    type;
    Price        price;          // 0 for market orders
    Quantity     remaining_qty;  // qty still unfilled
    Quantity     total_qty;      // original qty
    Quantity     visible_qty;    // iceberg visible portion (0 = full visibility)
    Timestamp    timestamp;      // for price-time priority

    Order(OrderId id, Side side, OrderType type, Price price,
          Quantity qty, Timestamp ts, Quantity visible = 0)
        : id(id), side(side), type(type), price(price),
          remaining_qty(qty), total_qty(qty), visible_qty(visible),
          timestamp(ts) {}

    Order() = default;

    bool isFilled() const { return remaining_qty == 0; }

    void fill(Quantity qty) {
        remaining_qty -= qty;
    }

    // Is this an iceberg order with hidden quantity?
    bool isIceberg() const { return visible_qty > 0 && visible_qty < total_qty; }

    // Current displayed quantity (for iceberg: min of visible_qty and remaining)
    Quantity displayQty() const {
        if (visible_qty == 0) return remaining_qty;
        return std::min(visible_qty, remaining_qty);
    }
};

// Trade result: when an order matches
struct Trade {
    OrderId  maker_id;
    OrderId  taker_id;
    Price    price;
    Quantity qty;
    Timestamp time;

    Trade(OrderId maker, OrderId taker, Price p, Quantity q, Timestamp t)
        : maker_id(maker), taker_id(taker), price(p), qty(q), time(t) {}
};

// Result of submitting an order to the matching engine
struct SubmitResult {
    OrderId                id;
    SubmitStatus           status;
    std::vector<Trade>     trades;
};
