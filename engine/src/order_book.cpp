#include "order_book.hpp"
#include <algorithm>
#include <numeric>
#include <iostream>

std::vector<Trade> OrderBook::addOrder(Order& taker_order) {
    std::vector<Trade> trades;
    Quantity remaining = taker_order.remaining_qty;

    if (taker_order.type == OrderType::Market) {
        // Market order: match against opposite side
        if (taker_order.side == Side::Bid) {
            while (remaining > 0 && !asks_.empty()) {
                auto it = asks_.begin();
                auto level_trades = it->second.fillWithTrades(remaining);
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
                auto level_trades = it->second.fillWithTrades(remaining);
                for (auto& [maker_id, qty] : level_trades) {
                    trades.emplace_back(maker_id, taker_order.id, it->first, qty, taker_order.timestamp);
                }
                remaining -= std::accumulate(level_trades.begin(), level_trades.end(), Quantity(0),
                    [](Quantity sum, auto& p) { return sum + p.second; });
                if (it->second.empty()) bids_.erase(it);
            }
        }
        taker_order.remaining_qty = remaining;
        return trades;
    }

    // Limit order: match while price crosses
    if (taker_order.side == Side::Bid) {
        while (remaining > 0 && !asks_.empty() && asks_.begin()->first <= taker_order.price) {
            auto it = asks_.begin();
            auto level_trades = it->second.fillWithTrades(remaining);
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
            auto level_trades = it->second.fillWithTrades(remaining);
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
    return true;
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
