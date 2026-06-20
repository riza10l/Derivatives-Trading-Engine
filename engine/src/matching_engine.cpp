#include "matching_engine.hpp"

SubmitResult MatchingEngine::submitOrder(Side side, OrderType type, Price price,
                                         Quantity qty, Quantity visible_qty) {
    OrderId id = next_order_id_++;
    auto timestamp = std::chrono::duration_cast<Timestamp>(
        std::chrono::system_clock::now().time_since_epoch()
    );

    // --- Post-Only: reject if it would cross the book ---
    if (type == OrderType::PostOnly) {
        if (book_.wouldMatch(side, price)) {
            return {id, SubmitStatus::Rejected, {}};
        }
        // Doesn't cross — add as resting limit order
        Order order{id, side, OrderType::Limit, price, qty, timestamp};
        book_.addToBook(order);
        return {id, SubmitStatus::Accepted, {}};
    }

    // --- Fill-or-Kill: must fill entire qty or cancel ---
    if (type == OrderType::FOK) {
        // Check if enough liquidity exists on the opposite side
        // availableQty takes the taker's side and checks the opposite book
        Quantity avail = book_.availableQty(side, price);
        if (avail < qty) {
            return {id, SubmitStatus::Cancelled, {}};
        }
        // Enough liquidity — match normally
        Order order{id, side, OrderType::Limit, price, qty, timestamp};
        std::vector<Trade> trades = book_.addOrder(order);
        total_trades_ += trades.size();
        recordTrades(trades);

        // FOK should be fully filled if availableQty was correct
        SubmitStatus status = order.isFilled() ? SubmitStatus::Filled : SubmitStatus::Cancelled;
        return {id, status, std::move(trades)};
    }

    // --- Immediate-or-Cancel: match what's available, cancel the rest ---
    if (type == OrderType::IOC) {
        Order order{id, side, OrderType::Limit, price, qty, timestamp};
        std::vector<Trade> trades = book_.addOrder(order);

        // addOrder may have placed remainder on the book — remove it
        if (order.remaining_qty > 0) {
            book_.cancelOrder(id);
        }

        total_trades_ += trades.size();
        recordTrades(trades);

        if (trades.empty()) {
            return {id, SubmitStatus::Cancelled, {}};
        } else if (order.remaining_qty == 0) {
            // Wait — remaining_qty was already updated by addOrder, but we
            // need to check the total filled vs original qty
            Quantity filled = 0;
            for (const auto& t : trades) filled += t.qty;
            if (filled >= qty) {
                return {id, SubmitStatus::Filled, std::move(trades)};
            }
            return {id, SubmitStatus::PartialFill, std::move(trades)};
        } else {
            return {id, SubmitStatus::PartialFill, std::move(trades)};
        }
    }

    // --- Iceberg: match visible portion, rest stays hidden ---
    if (type == OrderType::Iceberg) {
        if (visible_qty == 0 || visible_qty >= qty) {
            // Invalid iceberg config — treat as regular limit
            visible_qty = 0;
        }

        Order order{id, side, OrderType::Iceberg, price, qty, timestamp, visible_qty};

        // Track full iceberg state before matching
        if (visible_qty > 0) {
            book_.trackIceberg(order);
        }

        // Match normally — addOrder handles visible portion via displayQty()
        std::vector<Trade> trades = book_.addOrder(order);
        total_trades_ += trades.size();
        recordTrades(trades);

        Quantity filled = 0;
        for (const auto& t : trades) filled += t.qty;

        SubmitStatus status;
        if (filled >= qty) {
            status = SubmitStatus::Filled;
            book_.removeIcebergTracking(id);
        } else if (filled > 0) {
            status = SubmitStatus::Accepted;  // partially filled, rest resting
        } else {
            status = SubmitStatus::Accepted;  // resting in book
        }
        return {id, status, std::move(trades)};
    }

    // --- Limit / Market: standard behavior ---
    Order order{id, side, type, price, qty, timestamp};
    std::vector<Trade> trades = book_.addOrder(order);
    total_trades_ += trades.size();
    recordTrades(trades);

    SubmitStatus status;
    if (order.isFilled()) {
        status = SubmitStatus::Filled;
    } else if (!trades.empty()) {
        status = SubmitStatus::Accepted;  // partial fill + resting
    } else {
        status = SubmitStatus::Accepted;  // pure resting
    }
    return {id, status, std::move(trades)};
}

void MatchingEngine::recordTrades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        if (recent_trades_.size() >= MAX_RECENT_TRADES) {
            recent_trades_.pop_front();
        }
        recent_trades_.push_back(t);
    }
}

bool MatchingEngine::cancelOrder(OrderId id) {
    return book_.cancelOrder(id);
}
