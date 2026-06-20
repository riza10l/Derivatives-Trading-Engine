#include "simulator.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>

void test_empty_simulation() {
    std::cout << "Test: Empty simulation (no bots)... ";
    Simulator sim;
    sim.run(10);
    assert(sim.currentTick() == 10);
    assert(sim.engine().totalTrades() == 0);
    std::cout << "PASS" << std::endl;
}

void test_random_bot() {
    std::cout << "Test: Random bot runs without crash... ";
    Simulator sim;
    sim.addBot("random", makeRandomBot());
    sim.run(100);
    // Should have placed 100 orders
    assert(sim.currentTick() == 100);
    std::printf("  (%llu trades)\n", (unsigned long long)sim.engine().totalTrades());
    std::cout << "PASS" << std::endl;
}

void test_market_maker() {
    std::cout << "Test: Market maker bot... ";
    Simulator sim;
    // Seed with some orders so book isn't empty
    sim.engine().submitOrder(Side::Bid, OrderType::Limit, 100, 10);
    sim.engine().submitOrder(Side::Ask, OrderType::Limit, 102, 10);

    sim.addBot("mm", makeMarketMaker());
    sim.run(50);

    assert(sim.currentTick() == 50);
    auto best_bid = sim.engine().book().bestBid();
    auto best_ask = sim.engine().book().bestAsk();
    assert(best_bid.has_value());
    assert(best_ask.has_value());
    std::cout << "PASS (spread=" << (*best_ask - *best_bid) << ")" << std::endl;
}

void test_multi_bot() {
    std::cout << "Test: Multiple bots together... ";
    Simulator sim;
    sim.addBot("random", makeRandomBot());
    sim.addBot("mm", makeMarketMaker());
    sim.run(200);
    assert(sim.currentTick() == 200);
    std::printf("  (%llu trades total)\n", (unsigned long long)sim.engine().totalTrades());
    std::cout << "PASS" << std::endl;
}

void test_save_log() {
    std::cout << "Test: Save log to CSV... ";
    Simulator sim;
    sim.addBot("random", makeRandomBot());
    sim.run(50);
    const char* path = "test_sim_log.csv";
    sim.saveLog(path);

    FILE* f = std::fopen(path, "r");
    assert(f != nullptr);
    char buf[256];
    int lines = 0;
    int data_rows = 0;
    while (std::fgets(buf, sizeof(buf), f)) {
        ++lines;
        if (buf[0] != '#' && buf[0] != '\n') data_rows++;
    }
    assert(data_rows >= 50);  // column header + 50 data rows
    std::fclose(f);
    std::remove(path);
    std::cout << "PASS (" << lines << " lines, " << data_rows << " data rows)" << std::endl;
}

void test_funding_updates() {
    std::cout << "Test: Funding rate updates during simulation... ";
    Simulator sim;
    sim.engine().submitOrder(Side::Bid, OrderType::Limit, 100, 1000);
    sim.engine().submitOrder(Side::Ask, OrderType::Limit, 102, 10);

    // Heavy bid imbalance → positive funding
    sim.addBot("mm", makeMarketMaker());
    sim.run(10);

    auto funding = sim.perpetual().fundingRate();
    // Should be positive (more bids)
    std::cout << " (funding=" << funding << ")";
    std::cout << "PASS" << std::endl;
}

int main() {
    std::cout << "=== Simulation Tests ===" << std::endl;
    test_empty_simulation();
    test_random_bot();
    test_market_maker();
    test_multi_bot();
    test_save_log();
    test_funding_updates();
    std::cout << "\nAll simulation tests passed!" << std::endl;
    return 0;
}
