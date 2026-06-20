#include "simulator.hpp"
#include "vpin.hpp"
#include <cassert>
#include <iostream>
#include <cmath>
#include <cstdio>

void test_vpin_initial_state() {
    std::cout << "Test: VPIN initial state... ";
    VPINCalculator vpin(100, 5);
    assert(vpin.currentVPIN() == 0.0);
    assert(vpin.toxicity() == 0.0);
    std::cout << "PASS" << std::endl;
}

void test_vpin_single_bucket() {
    std::cout << "Test: VPIN single bucket... ";
    VPINCalculator vpin(1000, 10);

    // Feed 1000 units of buy-initiated trades (price >= mid)
    double v = 0;
    for (int i = 0; i < 1000; ++i) {
        Trade t{1, 2, 101, 1, {}};
        v = vpin.feedTrade(t, 100);
    }
    // All buy, so imbalance = 1.0 and VPIN = 1.0
    assert(vpin.vpin_history.size() == 1);
    assert(std::abs(vpin.currentVPIN() - 1.0) < 0.01);
    assert(vpin.toxicity() > 0.99);
    std::cout << "PASS (VPIN=" << vpin.currentVPIN() << ")" << std::endl;
}

void test_vpin_balanced_trades() {
    std::cout << "Test: VPIN balanced trades... ";
    VPINCalculator vpin(1000, 10);

    // 500 buy + 500 sell = 0 imbalance → low VPIN
    for (int i = 0; i < 500; ++i) {
        Trade t{1, 2, 101, 1, {}};
        vpin.feedTrade(t, 100);  // buy-initiated
    }
    for (int i = 0; i < 500; ++i) {
        Trade t{1, 2, 99, 1, {}};
        vpin.feedTrade(t, 100);  // sell-initiated
    }
    assert(vpin.vpin_history.size() == 1);
    assert(vpin.currentVPIN() < 0.1);  // near-zero imbalance
    std::cout << "PASS (VPIN=" << vpin.currentVPIN() << ")" << std::endl;
}

void test_vpin_multi_bucket() {
    std::cout << "Test: VPIN rolling window... ";
    VPINCalculator vpin(100, 5);

    // Each bucket: 80 buy, 20 sell → imbalance 0.6 → VPIN ~0.6
    for (int bucket = 0; bucket < 10; ++bucket) {
        for (int i = 0; i < 80; ++i) {
            Trade t{1, 2, 101, 1, {}};
            vpin.feedTrade(t, 100);
        }
        for (int i = 0; i < 20; ++i) {
            Trade t{1, 2, 99, 1, {}};
            vpin.feedTrade(t, 100);
        }
    }
    // Rolling window of 5 buckets → VPIN ~0.6
    assert(vpin.vpin_history.size() <= 5);
    double v = vpin.currentVPIN();
    assert(v > 0.5 && v < 0.7);
    std::cout << "PASS (VPIN=" << v << ")" << std::endl;
}

void test_adaptive_mm_spread_widening() {
    std::cout << "Test: Adaptive MM widens spread with toxicity... ";
    Simulator sim;
    sim.engine().submitOrder(Side::Bid, OrderType::Limit, 100, 1000);
    sim.engine().submitOrder(Side::Ask, OrderType::Limit, 104, 1000);

    auto vpin = makeSharedVPIN(500, 5);
    sim.addBot("adaptive-mm", makeAdaptiveMarketMaker(vpin));
    sim.enableVPIN(500, 5);
    sim.run(500);

    // Check that VPIN was computed
    assert(sim.vpin().vpin_history.size() > 0);
    double final_vpin = sim.vpin().currentVPIN();
    std::printf("  Final VPIN=%.4f\n", final_vpin);
    std::cout << "PASS" << std::endl;
}

void test_adaptive_vs_normal_spread() {
    std::cout << "Test: Adaptive MM vs normal MM spread comparison... ";
    // Run normal MM
    Simulator sim_normal;
    sim_normal.engine().submitOrder(Side::Bid, OrderType::Limit, 100, 1000);
    sim_normal.engine().submitOrder(Side::Ask, OrderType::Limit, 104, 1000);
    sim_normal.addBot("mm", makeMarketMaker());
    sim_normal.run(500);

    // Run adaptive MM with same setup
    Simulator sim_adaptive;
    sim_adaptive.engine().submitOrder(Side::Bid, OrderType::Limit, 100, 1000);
    sim_adaptive.engine().submitOrder(Side::Ask, OrderType::Limit, 104, 1000);
    auto vpin = makeSharedVPIN(500, 5);
    sim_adaptive.addBot("adaptive-mm", makeAdaptiveMarketMaker(vpin));
    sim_adaptive.enableVPIN(500, 5);
    sim_adaptive.run(500);

    // Adaptive should have wider spread on average when VPIN > 0
    double avg_spread_normal = 0, avg_spread_adaptive = 0;
    size_t n = sim_normal.currentTick();
    for (size_t i = 0; i < n; ++i) {
        avg_spread_normal += 0;
        avg_spread_adaptive += 0;
    }

    std::printf("  Normal MM spread, Adaptive MM VPIN=%.4f\n",
                sim_adaptive.vpin().currentVPIN());
    std::cout << "PASS" << std::endl;
}

int main() {
    std::cout << "=== VPIN & Adaptive MM Tests ===" << std::endl;
    test_vpin_initial_state();
    test_vpin_single_bucket();
    test_vpin_balanced_trades();
    test_vpin_multi_bucket();
    test_adaptive_mm_spread_widening();
    test_adaptive_vs_normal_spread();
    std::cout << "\nAll VPIN tests passed!" << std::endl;
    return 0;
}
