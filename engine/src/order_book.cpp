#include "order_book.hpp"
#include <algorithm>
#include <numeric>
#include <iostream>

std::vector<Trade> OrderBook::addOrder(Order& taker_order) {
    std::vector<Trade> trades;
    Quantity remaining = taker_order.remaining_qty;

    // Collect iceberg orders whose visible portion was fully consumed
    std::vector<Order> iceberg_consumed;

    if (taker_order.type == OrderType::Market) {
        // Market order: match against opposite side
        if (taker_order.side == Side::Bid) {
            while (remaining > 0 && !asks_.empty()) {
                auto it = asks_.begin();
                auto level_trades = it->second.fillWithTrades(remaining, &iceberg_consumed);
                for (auto& [maker_id, qty] : level_trades) {
                    trades.emplace_back(maker_id, taker_order.id, it->first, qty, taker_order.timestamp);
                }
                remaining -= std::accumulate(level_trades.begin(), level_trades.end(), Quantity(0),
                    [](Quantity sum, auto& p) { return sum + p.second; });
                if (it->second.empty()) asks_.erase(it);
            }
        } else {
            while (remaining > 0 && !bids_.empty()) {
                auto it = bids_.begin();
                auto level_trades = it->second.fillWithTrades(remaining, &iceberg_consumed);
                for (auto& [maker_id, qty] : level_trades) {
                    trades.emplace_back(maker_id, taker_order.id, it->first, qty, taker_order.timestamp);
                }
                remaining -= std::accumulate(level_trades.begin(), level_trades.end(), Quantity(0),
                    [](Quantity sum, auto& p) { return sum + p.second; });
                if (it->second.empty()) bids_.erase(it);
            }
        }
        taker_order.remaining_qty = remaining;
        refreshIcebergs(iceberg_consumed);
        return trades;
    }

    // Limit order: match while price crosses
    if (taker_order.side == Side::Bid) {
        while (remaining > 0 && !asks_.empty() && asks_.begin()->first <= taker_order.price) {
            auto it = asks_.begin();
            auto level_trades = it->second.fillWithTrades(remaining, &iceberg_consumed);
            for (auto& [maker_id, qty] : level_trades) {
                trades.emplace_back(maker_id, taker_order.id, it->first, qty, taker_order.timestamp);
            }
            remaining -= std::accumulate(level_trades.begin(), level_trades.end(), Quantity(0),
                [](Quantity sum, auto& p) { return sum + p.second; });
            if (it->second.empty()) asks_.erase(it);
        }
    } else {
        while (remaining > 0 && !bids_.empty() && bids_.begin()->first >= taker_order.price) {
            auto it = bids_.begin();
            auto level_trades = it->second.fillWithTrades(remaining, &iceberg_consumed);
            for (auto& [maker_id, qty] : level_trades) {
                trades.emplace_back(maker_id, taker_order.id, it->first, qty, taker_order.timestamp);
            }
            remaining -= std::accumulate(level_trades.begin(), level_trades.end(), Quantity(0),
                [](Quantity sum, auto& p) { return sum + p.second; });
            if (it->second.empty()) bids_.erase(it);
        }
    }

    taker_order.remaining_qty = remaining;
    if (taker_order.remaining_qty > 0) {
        addToBook(taker_order);
    }

    refreshIcebergs(iceberg_consumed);
    return trades;
}

bool OrderBook::addToBook(const Order& order) {
    if (order.side == Side::Bid) {
        bids_[order.price].addOrder(order);
    } else {
        asks_[order.price].addOrder(order);
    }
    orders_index_[order.id] = {order.side, order.price};
    return true;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto it = orders_index_.find(id);
    if (it == orders_index_.end()) return false;

    auto [side, price] = it->second;
    if (side == Side::Bid) {
        auto level_it = bids_.find(price);
        if (level_it != bids_.end()) {
            level_it->second.removeOrder(id);
            if (level_it->second.empty()) bids_.erase(level_it);
        }
    } else {
        auto level_it = asks_.find(price);
        if (level_it != asks_.end()) {
            level_it->second.removeOrder(id);
            if (level_it->second.empty()) asks_.erase(level_it);
        }
    }

    orders_index_.erase(it);
    removeIcebergTracking(id);
    return true;
}

bool OrderBook::wouldMatch(Side side, Price price) const {
    if (side == Side::Bid) {
        // A bid would match if there's an ask at or below the bid price
        if (asks_.empty()) return false;
        return asks_.begin()->first <= price;
    } else {
        // An ask would match if there's a bid at or above the ask price
        if (bids_.empty()) return false;
        return bids_.begin()->first >= price;
    }
}

Quantity OrderBook::availableQty(Side side, Price price) const {
    Quantity total = 0;

    if (side == Side::Bid) {
        // Quantity available on the ask side at or below price
        for (const auto& [ask_price, level] : asks_) {
            if (ask_price > price) break;  // asks are sorted ascending
            total += level.total_quantity;
        }
    } else {
        // Quantity available on the bid side at or above price
        for (const auto& [bid_price, level] : bids_) {
            if (bid_price < price) break;  // bids are sorted descending
            total += level.total_quantity;
        }
    }

    // Also account for hidden iceberg quantity that would become available
    // during matching (iceberg refreshes happen mid-match)
    for (const auto& [id, state] : icebergs_) {
        if (state.original.side != side) continue;  // wrong side
        // Check if this iceberg's price is "good enough"
        if (side == Side::Bid && state.original.price > price) continue;
        if (side == Side::Ask && state.original.price < price) continue;
        total += state.hidden_remaining;
    }

    return total;
}

std::optional<Price> OrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const {
    auto bid = bestBid();
    auto ask = bestAsk();
    if (!bid || !ask) return std::nullopt;
    return *ask - *bid;
}

Quantity OrderBook::depthAtPrice(Price price, Side side) const {
    if (side == Side::Bid) {
        auto it = bids_.find(price);
        return (it == bids_.end()) ? 0 : it->second.total_quantity;
    } else {
        auto it = asks_.find(price);
        return (it == asks_.end()) ? 0 : it->second.total_quantity;
    }
}

Quantity OrderBook::totalBidQty() const {
    Quantity total = 0;
    for (const auto& [_, level] : bids_) total += level.total_quantity;
    return total;
}

Quantity OrderBook::totalAskQty() const {
    Quantity total = 0;
    for (const auto& [_, level] : asks_) total += level.total_quantity;
    return total;
}

double OrderBook::imbalance() const {
    int64_t bid_qty = totalBidQty();
    int64_t ask_qty = totalAskQty();
    int64_t total = bid_qty + ask_qty;
    if (total == 0) return 0.0;
    return static_cast<double>(bid_qty - ask_qty) / static_cast<double>(total);
}

void OrderBook::dump() const {
    std::cout << "=== BIDS ===" << std::endl;
    for (const auto& [price, level] : bids_) {
        std::cout << "  " << price << " | " << level.total_quantity << std::endl;
    }
    std::cout << "=== ASKS ===" << std::endl;
    for (const auto& [price, level] : asks_) {
        std::cout << "  " << price << " | " << level.total_quantity << std::endl;
    }
}

void OrderBook::trackIceberg(const Order& order) {
    IcebergState state;
    state.original = order;
    // Hidden remaining = total - visible portion placed on book
    Quantity on_book = order.displayQty();
    state.hidden_remaining = order.remaining_qty > on_book ? order.remaining_qty - on_book : 0;
    icebergs_[order.id] = state;
}

void OrderBook::removeIcebergTracking(OrderId id) {
    icebergs_.erase(id);
}

void OrderBook::refreshIcebergs(const std::vector<Order>& consumed_icebergs) {
    for (const auto& consumed : consumed_icebergs) {
        auto it = icebergs_.find(consumed.id);
        if (it == icebergs_.end()) continue;

        auto& state = it->second;
        if (state.hidden_remaining == 0) {
            // Fully consumed
            icebergs_.erase(it);
            orders_index_.erase(consumed.id);
            continue;
        }

        // Refresh: place a new visible slice at the back of the queue
        Quantity new_visible = std::min(state.original.visible_qty, state.hidden_remaining);
        state.hidden_remaining -= new_visible;

        Order refreshed = state.original;
        refreshed.remaining_qty = new_visible;

        // Use addOrderRaw so we place exactly new_visible on the book
        // (not going through displayQty() again)
        if (refreshed.side == Side::Bid) {
            bids_[refreshed.price].addOrderRaw(refreshed);
        } else {
            asks_[refreshed.price].addOrderRaw(refreshed);
        }
        // orders_index_ already has this id; update is idempotent
        orders_index_[refreshed.id] = {refreshed.side, refreshed.price};
    }
}
