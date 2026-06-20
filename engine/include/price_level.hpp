#pragma once

#include "order.hpp"
#include <list>
#include <cstdint>

// Price level: all orders at same price, FIFO queue
// alignas(64): start address is cache-aligned.
// ponytail: padding[7] removed — achieve cache-line isolation only when
// profiling shows false sharing. alignas(64) alone gives the alignment.
struct alignas(64) PriceLevel {
    Price              price;
    Quantity           total_quantity;   // sum of all orders at this level
    uint32_t           order_count;

    PriceLevel() : price(0), total_quantity(0), order_count(0) {}
    explicit PriceLevel(Price p) : price(p), total_quantity(0), order_count(0) {}

    void addOrder(const Order& order) {
        // For iceberg orders, only the visible portion is "on the book"
        Quantity display = order.displayQty();
        Order book_order = order;
        book_order.remaining_qty = display;
        orders.push_back(book_order);
        total_quantity += display;
        ++order_count;
    }

    // Add an iceberg order preserving its full remaining_qty in the list
    // (used internally for refresh — the order tracks its own hidden state)
    void addOrderRaw(const Order& order) {
        orders.push_back(order);
        total_quantity += order.remaining_qty;
        ++order_count;
    }

    // Fill by taker quantity, return list of (order_id, filled_qty) pairs
    // Also populates iceberg_refreshes: orders whose visible portion was
    // fully consumed but still have hidden quantity remaining.
    std::list<std::pair<OrderId, Quantity>> fillWithTrades(
        Quantity qty,
        std::vector<Order>* iceberg_refreshes = nullptr)
    {
        std::list<std::pair<OrderId, Quantity>> trades;
        Quantity remaining = qty;

        while (remaining > 0 && !orders.empty()) {
            Order& order = orders.front();
            Quantity can_fill = std::min(remaining, order.remaining_qty);
            order.fill(can_fill);
            trades.emplace_back(order.id, can_fill);
            total_quantity -= can_fill;
            remaining -= can_fill;

            if (order.isFilled()) {
                // Check if this is an iceberg with hidden quantity left
                if (order.isIceberg() && iceberg_refreshes) {
                    // The order in the book only had visible portion;
                    // caller must check the original order's total remaining
                    iceberg_refreshes->push_back(order);
                }
                orders.pop_front();
                --order_count;
            }
        }
        return trades;
    }

    void removeOrder(OrderId id) {
        for (auto it = orders.begin(); it != orders.end(); ++it) {
            if (it->id == id) {
                total_quantity -= it->remaining_qty;
                orders.erase(it);
                --order_count;
                return;
            }
        }
    }

    bool empty() const { return orders.empty(); }
    Quantity frontQty() const { return orders.empty() ? 0 : orders.front().remaining_qty; }

    // Access to orders list for iceberg refresh and inspection
    const std::list<Order>& getOrders() const { return orders; }

private:
    std::list<Order> orders;      // FIFO queue per price level
};
