#include "matching_engine.hpp"
#include "perpetual.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

static constexpr auto EPS = 1e-9;

void test_mid_price_from_book() {
    std::cout << "Test: Mid price from order book... ";
    MatchingEngine engine;

    // Bid 100, Ask 102 → mid = 101
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    engine.submitOrder(Side::Ask, OrderType::Limit, 102, 10);

    PerpetualEngine pe(&engine.book());
    pe.tick();

    assert(pe.markPrice() == 101);
    std::cout << "PASS (mark=" << pe.markPrice() << ")" << std::endl;
}

void test_mid_price_rounds_down() {
    std::cout << "Test: Mid price rounds down (integer)... ";
    MatchingEngine engine;

    // Bid 100, Ask 103 → mid = (100+103)/2 = 101 (integer division)
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    engine.submitOrder(Side::Ask, OrderType::Limit, 103, 10);

    PerpetualEngine pe(&engine.book());
    pe.tick();

    assert(pe.markPrice() == 101);
    std::cout << "PASS (mark=" << pe.markPrice() << ")" << std::endl;
}

void test_preserves_last_mark_on_empty_book() {
    std::cout << "Test: Preserve mark price when book empties... ";
    MatchingEngine engine;

    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    engine.submitOrder(Side::Ask, OrderType::Limit, 102, 10);

    PerpetualEngine pe(&engine.book());
    pe.tick();
    assert(pe.markPrice() == 101);

    // Cancel all orders
    engine.cancelOrder(1);
    engine.cancelOrder(2);

    pe.tick();  // book empty, harus preserve 101
    assert(pe.markPrice() == 101);
    std::cout << "PASS" << std::endl;
}

void test_funding_rate_positive_on_bid_dominated() {
    std::cout << "Test: Funding rate positive when more bids... ";
    MatchingEngine engine;

    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 100);  // heavy bid
    engine.submitOrder(Side::Ask, OrderType::Limit, 102, 10);   // light ask

    PerpetualEngine pe(&engine.book());
    pe.tick();

    assert(pe.fundingRate() > 0);
    std::cout << "PASS (funding=" << pe.fundingRate() << ")" << std::endl;
}

void test_funding_rate_negative_on_ask_dominated() {
    std::cout << "Test: Funding rate negative when more asks... ";
    MatchingEngine engine;

    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 10);   // light bid
    engine.submitOrder(Side::Ask, OrderType::Limit, 102, 100);  // heavy ask

    PerpetualEngine pe(&engine.book());
    pe.tick();

    assert(pe.fundingRate() < 0);
    std::cout << "PASS (funding=" << pe.fundingRate() << ")" << std::endl;
}

void test_funding_rate_zero_on_balanced() {
    std::cout << "Test: Funding rate zero when balanced... ";
    MatchingEngine engine;

    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 50);
    engine.submitOrder(Side::Ask, OrderType::Limit, 102, 50);

    PerpetualEngine pe(&engine.book());
    pe.tick();

    assert(std::fabs(pe.fundingRate()) < EPS);
    std::cout << "PASS" << std::endl;
}

void test_funding_rate_clamped() {
    std::cout << "Test: Funding rate clamped to max... ";
    MatchingEngine engine;

    // Extreme imbalance: sensitivity * imbalance > max
    engine.submitOrder(Side::Bid, OrderType::Limit, 100, 999999);
    engine.submitOrder(Side::Ask, OrderType::Limit, 102, 1);

    PerpetualConfig cfg;
    cfg.max_funding_rate = 0.0005;  // 0.05%
    PerpetualEngine pe(&engine.book(), cfg);
    pe.tick();

    assert(std::fabs(pe.fundingRate()) <= cfg.max_funding_rate + EPS);
    std::cout << "PASS (funding=" << pe.fundingRate() << ")" << std::endl;
}

void test_trade_price_update() {
    std::cout << "Test: Last trade price updates via onTrade... ";
    MatchingEngine engine;
    PerpetualEngine pe(&engine.book());

    pe.onTrade(10150);
    assert(pe.lastTradePrice() == 10150);

    pe.onTrade(10200);
    assert(pe.lastTradePrice() == 10200);

    std::cout << "PASS" << std::endl;
}

void test_mark_price_clamped_against_trade() {
    std::cout << "Test: Mark price clamped within premium of trade price... ";
    MatchingEngine engine;
    PerpetualConfig cfg;
    cfg.max_premium = 0.01;       // 1%
    cfg.tick_size = 100;          // price in cents: BTC = 50000.00
    PerpetualEngine pe(&engine.book(), cfg);

    // Set last trade = 50000 (in cents: 50,000.00)
    pe.onTrade(50000);

    // Populate book dengan bid/ask = 55000/56000 → mid = 55500 (jauh dari trade)
    engine.submitOrder(Side::Bid, OrderType::Limit, 55000, 10);
    engine.submitOrder(Side::Ask, OrderType::Limit, 56000, 10);

    pe.tick();

    // max_premium = 1% of 50000 = 500
    // mark price harus di [50000-500, 50000+500] = [49500, 50500]
    Price max_allowed = 50000 + static_cast<Price>(50000 * cfg.max_premium);
    Price min_allowed = 50000 - static_cast<Price>(50000 * cfg.max_premium);

    assert(pe.markPrice() <= max_allowed);
    assert(pe.markPrice() >= min_allowed);
    std::cout << "PASS (mark=" << pe.markPrice()
              << " range=[" << min_allowed << "," << max_allowed << "])" << std::endl;
}

void test_funding_with_ticker() {
    std::cout << "Test: Tick count increments... ";
    MatchingEngine engine;
    PerpetualEngine pe(&engine.book());

    assert(pe.tickCount() == 0);
    pe.tick();
    assert(pe.tickCount() == 1);
    pe.tick();
    assert(pe.tickCount() == 2);
    std::cout << "PASS" << std::endl;
}

void test_mark_price_from_trade_when_book_empty() {
    std::cout << "Test: Mark price falls back to last trade when book empty... ";
    MatchingEngine engine;
    PerpetualEngine pe(&engine.book());

    pe.onTrade(20000);
    pe.tick();  // book masih kosong
    assert(pe.markPrice() == 20000);
    std::cout << "PASS" << std::endl;
}

int main() {
    std::cout << "=== Perpetual Engine Tests ===" << std::endl;
    test_mid_price_from_book();
    test_mid_price_rounds_down();
    test_preserves_last_mark_on_empty_book();
    test_funding_rate_positive_on_bid_dominated();
    test_funding_rate_negative_on_ask_dominated();
    test_funding_rate_zero_on_balanced();
    test_funding_rate_clamped();
    test_trade_price_update();
    test_mark_price_clamped_against_trade();
    test_funding_with_ticker();
    test_mark_price_from_trade_when_book_empty();
    std::cout << "\nAll perpetual engine tests passed!" << std::endl;
    return 0;
}
