#include "matching_engine.hpp"
#include <cassert>
#include <iostream>

// ==========================================================================
// Existing tests — updated to use SubmitResult
// ==========================================================================

void test_basic_limit_order_match() {
    std::cout << "Test: Basic limit order match... ";
    MatchingEngine engine;

    // Bid at 100, ask at 100 -> should match
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    auto result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 5);

    assert(result.trades.size() == 1);
    assert(result.trades[0].price == 100);
    assert(result.trades[0].qty == 5);
    assert(result.status == SubmitStatus::Filled);
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
    auto result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 3);
    assert(result.trades.size() == 1);
    assert(result.trades[0].maker_id == 1);
    assert(result.trades[0].qty == 3);

    // Ask at 100 qty 3: 2 from first order (remaining), 1 from second
    result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 3);
    assert(result.trades.size() == 2);
    assert(result.trades[0].maker_id == 1);
    assert(result.trades[1].maker_id == 2);
    std::cout << "PASS" << std::endl;
}

void test_partial_fill() {
    std::cout << "Test: Partial fill... ";
    MatchingEngine engine;

    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    auto result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 3);

    assert(result.trades.size() == 1);
    assert(result.trades[0].qty == 3);
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
    auto result = engine.submitOrder(Side::Bid, OrderType::Market, 0, 3);
    assert(result.trades.size() == 1);
    assert(result.trades[0].price == 100);
    assert(result.trades[0].qty == 3);
    std::cout << "PASS" << std::endl;
}

void test_cancel_order() {
    std::cout << "Test: Cancel order... ";
    MatchingEngine engine;

    auto result = engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    assert(*engine.book().bestBid() == 100);

    bool cancelled = engine.cancelOrder(result.id);
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

// ==========================================================================
// Week 3 — Advanced Order Type Tests
// ==========================================================================

void test_post_only_reject() {
    std::cout << "Test: Post-Only reject (would cross)... ";
    MatchingEngine engine;

    // Place a bid at 100
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);

    // Post-Only ask at 100 would cross the bid -> should be rejected
    auto result = engine.submitOrder(Side::Ask, OrderType::PostOnly, 100, 5);
    assert(result.status == SubmitStatus::Rejected);
    assert(result.trades.empty());

    // Book should be unchanged: bid still at 100 with 10
    assert(*engine.book().bestBid() == 100);
    assert(engine.book().depthAtPrice(100, Side::Bid) == 10);
    std::cout << "PASS" << std::endl;
}

void test_post_only_accept() {
    std::cout << "Test: Post-Only accept (no cross)... ";
    MatchingEngine engine;

    // Place an ask at 101
    engine.submitOrder(Side::Ask, OrderType::Limit, 101, 10);

    // Post-Only bid at 100 doesn't cross -> accepted, rests in book
    auto result = engine.submitOrder(Side::Bid, OrderType::PostOnly, 100, 5);
    assert(result.status == SubmitStatus::Accepted);
    assert(result.trades.empty());

    // Bid should be in the book
    assert(*engine.book().bestBid() == 100);
    assert(engine.book().depthAtPrice(100, Side::Bid) == 5);
    std::cout << "PASS" << std::endl;
}

void test_fok_reject() {
    std::cout << "Test: FOK reject (insufficient liquidity)... ";
    MatchingEngine engine;

    // Place 30 qty on ask side
    engine.submitOrder(Side::Ask, OrderType::Limit, 100, 30);

    // FOK bid for 50 — not enough liquidity -> cancel
    auto result = engine.submitOrder(Side::Bid, OrderType::FOK, 100, 50);
    assert(result.status == SubmitStatus::Cancelled);
    assert(result.trades.empty());

    // Book unchanged: ask still has 30
    assert(engine.book().depthAtPrice(100, Side::Ask) == 30);
    std::cout << "PASS" << std::endl;
}

void test_fok_accept() {
    std::cout << "Test: FOK accept (full fill)... ";
    MatchingEngine engine;

    // Place exactly 50 qty on ask side across two levels
    engine.submitOrder(Side::Ask, OrderType::Limit, 100, 30);
    engine.submitOrder(Side::Ask, OrderType::Limit, 101, 20);

    // FOK bid for 50 at price 101 — enough liquidity
    auto result = engine.submitOrder(Side::Bid, OrderType::FOK, 101, 50);
    assert(result.status == SubmitStatus::Filled);
    assert(!result.trades.empty());

    // Verify total filled qty
    Quantity total_filled = 0;
    for (const auto& t : result.trades) total_filled += t.qty;
    assert(total_filled == 50);

    // Book should be empty on ask side
    assert(engine.book().bestAsk() == std::nullopt);
    std::cout << "PASS" << std::endl;
}

void test_ioc_partial() {
    std::cout << "Test: IOC partial fill... ";
    MatchingEngine engine;

    // Place 30 qty on ask side
    engine.submitOrder(Side::Ask, OrderType::Limit, 100, 30);

    // IOC bid for 50 at 100 — match 30, cancel remaining 20
    auto result = engine.submitOrder(Side::Bid, OrderType::IOC, 100, 50);
    assert(result.status == SubmitStatus::PartialFill);

    Quantity total_filled = 0;
    for (const auto& t : result.trades) total_filled += t.qty;
    assert(total_filled == 30);

    // Remaining 20 should NOT be in the book
    assert(engine.book().bestBid() == std::nullopt);
    assert(engine.book().bestAsk() == std::nullopt);
    std::cout << "PASS" << std::endl;
}

void test_ioc_no_match() {
    std::cout << "Test: IOC no match (empty book)... ";
    MatchingEngine engine;

    // IOC on empty book — nothing to match
    auto result = engine.submitOrder(Side::Bid, OrderType::IOC, 100, 10);
    assert(result.status == SubmitStatus::Cancelled);
    assert(result.trades.empty());
    assert(engine.book().bestBid() == std::nullopt);
    std::cout << "PASS" << std::endl;
}

void test_ioc_full_fill() {
    std::cout << "Test: IOC full fill... ";
    MatchingEngine engine;

    engine.submitOrder(Side::Ask, OrderType::Limit, 100, 50);

    auto result = engine.submitOrder(Side::Bid, OrderType::IOC, 100, 30);
    assert(result.status == SubmitStatus::Filled);

    Quantity total_filled = 0;
    for (const auto& t : result.trades) total_filled += t.qty;
    assert(total_filled == 30);

    // Ask side should still have 20
    assert(engine.book().depthAtPrice(100, Side::Ask) == 20);
    std::cout << "PASS" << std::endl;
}

void test_iceberg_basic() {
    std::cout << "Test: Iceberg basic (visible portion on book)... ";
    MatchingEngine engine;

    // Submit iceberg: total=50, visible=10
    auto result = engine.submitOrder(Side::Bid, OrderType::Iceberg, 100, 50, 10);
    assert(result.status == SubmitStatus::Accepted);

    // Book should show only visible portion = 10
    assert(engine.book().depthAtPrice(100, Side::Bid) == 10);
    std::cout << "PASS" << std::endl;
}

void test_iceberg_refresh() {
    std::cout << "Test: Iceberg refresh after fill... ";
    MatchingEngine engine;

    // Submit iceberg bid: total=50, visible=10
    engine.submitOrder(Side::Bid, OrderType::Iceberg, 100, 50, 10);

    // Sell 10 — should fill the visible portion, then refresh to next 10
    auto result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 10);
    assert(!result.trades.empty());
    Quantity filled = 0;
    for (const auto& t : result.trades) filled += t.qty;
    assert(filled == 10);

    // After refresh, book should show 10 again (from the hidden 40)
    assert(engine.book().depthAtPrice(100, Side::Bid) == 10);
    std::cout << "PASS" << std::endl;
}

void test_iceberg_full_drain() {
    std::cout << "Test: Iceberg full drain (5 refresh cycles)... ";
    MatchingEngine engine;

    // Submit iceberg bid: total=50, visible=10
    engine.submitOrder(Side::Bid, OrderType::Iceberg, 100, 50, 10);

    Quantity total_filled = 0;
    for (int i = 0; i < 5; ++i) {
        auto result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 10);
        for (const auto& t : result.trades) total_filled += t.qty;
    }

    assert(total_filled == 50);
    // Book should be empty now
    assert(engine.book().bestBid() == std::nullopt);
    std::cout << "PASS" << std::endl;
}

void test_iceberg_partial_refresh() {
    std::cout << "Test: Iceberg partial fill within visible... ";
    MatchingEngine engine;

    // Submit iceberg bid: total=50, visible=10
    engine.submitOrder(Side::Bid, OrderType::Iceberg, 100, 50, 10);

    // Sell only 5 — should partially fill visible, no refresh yet
    auto result = engine.submitOrder(Side::Ask, OrderType::Limit, 100, 5);
    Quantity filled = 0;
    for (const auto& t : result.trades) filled += t.qty;
    assert(filled == 5);

    // Book should show remaining 5 of visible portion
    assert(engine.book().depthAtPrice(100, Side::Bid) == 5);
    std::cout << "PASS" << std::endl;
}

int main() {
    std::cout << "=== Matching Engine Tests ===" << std::endl;

    // Original tests
    test_basic_limit_order_match();
    test_price_time_priority();
    test_partial_fill();
    test_market_order();
    test_cancel_order();
    test_order_book_imbalance();

    // Week 3: Advanced order types
    std::cout << "\n=== Post-Only Tests ===" << std::endl;
    test_post_only_reject();
    test_post_only_accept();

    std::cout << "\n=== Fill-or-Kill Tests ===" << std::endl;
    test_fok_reject();
    test_fok_accept();

    std::cout << "\n=== Immediate-or-Cancel Tests ===" << std::endl;
    test_ioc_partial();
    test_ioc_no_match();
    test_ioc_full_fill();

    std::cout << "\n=== Iceberg Tests ===" << std::endl;
    test_iceberg_basic();
    test_iceberg_refresh();
    test_iceberg_full_drain();
    test_iceberg_partial_refresh();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
