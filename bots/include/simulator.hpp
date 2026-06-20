#pragma once

#include "matching_engine.hpp"
#include "perpetual.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <functional>

// ponytail: Bot adalah callback ke simulator — no class hierarchy, no virtual.
// Kalau strategi bot >3 macam, refactor jadi class.

using BotFunc = std::function<void(MatchingEngine&, uint64_t tick)>;

// Built-in bot factories
BotFunc makeRandomBot();
BotFunc makeMarketMaker();

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
};

class Simulator {
public:
    Simulator(PerpetualConfig perp_cfg = {});

    void addBot(const std::string& name, BotFunc fn);
    void run(uint64_t ticks);
    void saveLog(const std::string& path) const;

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
};
