#include "simulator.hpp"
#include <cstdio>
#include <ctime>
#include <random>
#include <cmath>

static std::mt19937& globalRng() {
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    return rng;
}

// =========================================================================
// Built-in bot factories
// =========================================================================

BotFunc makeRandomBot() {
    return [](MatchingEngine& engine, uint64_t) {
        auto& g = globalRng();
        std::uniform_int_distribution<int> side(0, 1);
        std::uniform_int_distribution<Price> price(95, 105);
        std::uniform_int_distribution<Quantity> qty(1, 10);

        engine.submitOrder(
            side(g) ? Side::Bid : Side::Ask,
            OrderType::Limit,
            price(g),
            qty(g)
        );
    };
}

BotFunc makeMarketMaker() {
    return [](MatchingEngine& engine, uint64_t) {
        auto& g = globalRng();
        auto best_bid = engine.book().bestBid();
        auto best_ask = engine.book().bestAsk();
        if (!best_bid || !best_ask) return;

        Price mid = (*best_bid + *best_ask) / 2;
        Price spread = *best_ask - *best_bid;
        if (spread < 2) spread = 2;

        std::uniform_int_distribution<Price> jitter(-1, 1);
        std::uniform_int_distribution<Quantity> qty(5, 15);

        Price bid_px = mid - spread / 2 + jitter(g);
        Price ask_px = mid + spread / 2 + jitter(g);

        // Prevent inverted quote
        if (bid_px >= ask_px) {
            bid_px = mid - 1;
            ask_px = mid + 1;
        }

        engine.submitOrder(Side::Bid, OrderType::Limit, bid_px, qty(g));
        engine.submitOrder(Side::Ask, OrderType::Limit, ask_px, qty(g));
    };
}

// =========================================================================
// Simulator
// =========================================================================

Simulator::Simulator(PerpetualConfig perp_cfg)
    : perpetual_(&engine_.book(), perp_cfg)
    , tick_(0) {}

void Simulator::addBot(const std::string& name, BotFunc fn) {
    bots_.emplace_back(name, std::move(fn));
}

void Simulator::run(uint64_t ticks) {
    for (uint64_t t = 0; t < ticks; ++t) {
        ++tick_;
        for (auto& [_, fn] : bots_) {
            fn(engine_, tick_);
        }
        perpetual_.tick();

        // Snapshot book state at end of tick
        TickSnapshot snap;
        snap.tick         = tick_;
        snap.best_bid     = engine_.book().bestBid().value_or(0);
        snap.best_ask     = engine_.book().bestAsk().value_or(0);
        snap.spread       = engine_.book().spread().value_or(0);
        snap.funding_rate = perpetual_.fundingRate();
        snap.mark_price   = perpetual_.markPrice();
        snap.imbalance    = engine_.book().imbalance();
        snap.total_trades = engine_.totalTrades();
        snapshots_.push_back(snap);
    }
    std::printf("[sim] %llu ticks, %zu bots, %llu trades\n",
                (unsigned long long)tick_, bots_.size(),
                (unsigned long long)engine_.totalTrades());
}

void Simulator::saveLog(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;

    std::fprintf(f, "# trading-engine simulation log\n");
    std::fprintf(f, "# ticks=%llu bots=%zu trades=%llu\n",
                 (unsigned long long)tick_, bots_.size(),
                 (unsigned long long)engine_.totalTrades());

    std::fprintf(f, "tick,bid,ask,spread,funding_rate,mark_price,"
                    "imbalance,total_trades\n");

    for (const auto& snap : snapshots_) {
        std::fprintf(f, "%llu,%lld,%lld,%lld,%.6f,%lld,%.4f,%llu\n",
            (unsigned long long)snap.tick,
            (long long)snap.best_bid,
            (long long)snap.best_ask,
            (long long)snap.spread,
            snap.funding_rate,
            (long long)snap.mark_price,
            snap.imbalance,
            (unsigned long long)snap.total_trades);
    }
    std::fclose(f);
}
