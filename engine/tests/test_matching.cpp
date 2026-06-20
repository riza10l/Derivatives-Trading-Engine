#include "matching_engine.hpp"
#include <cassert>
#include <iostream>

void test_basic_limit_order_match() {
    std::cout << "Test: Basic limit order match... ";
    MatchingEngine engine;

    // Bid at 100, ask at 100 -> should match
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    auto trades = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 5);

    assert(trades.size() == 1);
    assert(trades[0].price == 100);
    assert(trades[0].qty == 5);
    assert(*engine.book().bestBid() == 100); // bid partially filled: 5 remaining
    assert(engine.book().bestAsk() == std::nullopt); // ask fully filled
    std::cout << "PASS" << std::endl;
}

void test_price_time_priority() {
    std::cout << "Test: Price-time priority... ";
    MatchingEngine engine;

    // First bid at 100 (should match first)
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 5);
    // Second bid at 100 (later, FIFO in same price level)
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 3);

    // Ask at 100 qty 3 should fill against first bid
    auto trades = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 3);
    assert(trades.size() == 1);
    assert(trades[0].maker_id == 1);
    assert(trades[0].qty == 3);

    // Ask at 100 qty 3: 2 from first order (remaining), 1 from second
    trades = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 3);
    assert(trades.size() == 2);
    assert(trades[0].maker_id == 1);
    assert(trades[1].maker_id == 2);
    std::cout << "PASS" << std::endl;
}

void test_partial_fill() {
    std::cout << "Test: Partial fill... ";
    MatchingEngine engine;

    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    auto trades = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 3);

    assert(trades.size() == 1);
    assert(trades[0].qty == 3);
    assert(*engine.book().bestBid() == 100);
    assert(engine.book().depthAtPrice(100, Side::Bid) == 7); // 10 - 3 remaining
    std::cout << "PASS" << std::endl;
}

void test_market_order() {
    std::cout << "Test: Market order... ";
    MatchingEngine engine;

    // Set up some liquidity
    engine.submitOrder(Side::Ask, OrderType::Limit, 100, 5);
    engine.submitOrder(Side::Ask, OrderType::Limit, 101, 5);

    // Market buy should take best ask first
    auto trades = engine.submitOrder(Side::Bid, OrderType::Market, 0, 3);
    assert(trades.size() == 1);
    assert(trades[0].price == 100);
    assert(trades[0].qty == 3);
    std::cout << "PASS" << std::endl;
}

void test_cancel_order() {
    std::cout << "Test: Cancel order... ";
    MatchingEngine engine;

    auto trades = engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    assert(*engine.book().bestBid() == 100);

    bool cancelled = engine.cancelOrder(1);
    assert(cancelled);
    assert(engine.book().bestBid() == std::nullopt);
    std::cout << "PASS" << std::endl;
}

void test_order_book_imbalance() {
    std::cout << "Test: Order book imbalance... ";
    MatchingEngine engine;

    // More bids than asks -> positive imbalance
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 100);
    engine.submitOrder(Side::Ask, OrderType::Limit, 101, 20);

    double imbalance = engine.book().imbalance();
    assert(imbalance > 0); // more bids
    std::cout << "PASS (imbalance=" << imbalance << ")" << std::endl;
}

int main() {
    std::cout << "=== Matching Engine Tests ===" << std::endl;
    test_basic_limit_order_match();
    test_price_time_priority();
    test_partial_fill();
    test_market_order();
    test_cancel_order();
    test_order_book_imbalance();
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
