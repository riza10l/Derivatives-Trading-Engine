#include "matching_engine.hpp"

std::vector<Trade> MatchingEngine::submitOrder(Side side, OrderType type, Price price, Quantity qty) {
    OrderId id = next_order_id_++;
    auto timestamp = std::chrono::duration_cast<Timestamp>(
        std::chrono::system_clock::now().time_since_epoch()
    );

    Order order{id, side, type, price, qty, timestamp};
    std::vector<Trade> trades = book_.addOrder(order);

    total_trades_ += trades.size();
    return trades;
}

bool MatchingEngine::cancelOrder(OrderId id) {
    return book_.cancelOrder(id);
}
