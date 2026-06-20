#pragma once

#include "matching_engine.hpp"
#include "perpetual.hpp"
#include "vpin.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <memory>

// ponytail: Bot adalah callback ke simulator — no class hierarchy, no virtual.
// Kalau strategi bot >3 macam, refactor jadi class.

using BotFunc = std::function<void(MatchingEngine&, uint64_t tick)>;

// Built-in bot factories
BotFunc makeRandomBot();
BotFunc makeMarketMaker();
BotFunc makeAdaptiveMarketMaker(std::shared_ptr<VPINCalculator> vpin);

// Helper: create shared VPIN for cross-bot coordination
std::shared_ptr<VPINCalculator> makeSharedVPIN(Quantity bucket_vol = 1000, size_t window = 50);

// Per-tick book snapshot
struct TickSnapshot {
    uint64_t tick;
    Price    best_bid;
    Price    best_ask;
    Price    spread;
    double   funding_rate;
    Price    mark_price;
    double   imbalance;
    uint64_t total_trades;
    double   vpin = 0.0;       // current VPIN (0 if not tracked)
    double   toxicity = 0.0;   // normalized toxicity 0-1
};

class Simulator {
public:
    Simulator(PerpetualConfig perp_cfg = {});

    void addBot(const std::string& name, BotFunc fn);
    void run(uint64_t ticks);
    void saveLog(const std::string& path) const;

    // VPIN tracking: feed trades from the engine
    void enableVPIN(Quantity bucket_vol = 1000, size_t window = 50);
    const VPINCalculator& vpin() const { return *vpin_; }
          VPINCalculator& vpin()       { return *vpin_; }

    const MatchingEngine& engine() const { return engine_; }
          MatchingEngine& engine()       { return engine_; }
    const PerpetualEngine& perpetual() const { return perpetual_; }
    uint64_t currentTick() const { return tick_; }

private:
    MatchingEngine   engine_;
    PerpetualEngine  perpetual_;
    std::vector<std::pair<std::string, BotFunc>> bots_;
    std::vector<TickSnapshot> snapshots_;
    uint64_t tick_ = 0;
    std::shared_ptr<VPINCalculator> vpin_; // null = disabled
};
