# Derivatives Trading Engine

**Matching Engine, Microstructure Research, dan ML/RL Market Making**

Sebuah simulasi exchange derivatif (perpetual futures) dari nol — matching engine C++20, mekanisme perpetual, multi-agent simulation, dan layer riset kuantitatif. Solo summer project.

---

## Fitur

### Core Engine (C++20)
- **Matching Engine** — price-time priority, `std::map` BST, FIFO per level
- **Cache-aligned struct** — `alignas(64)` anti false sharing
- **6 order types** — Limit, Market, Post-Only, FOK, IOC, Iceberg
- **SPSC lock-free ring buffer** — LMAX Disruptor pattern

### Perpetual Mechanics
- **Mark Price** — weighted mid + last trade, clipped
- **Funding Rate** — Order Book Imbalance, otomatis tiap tick

### Network Layer
- **Binary protocol** — packed struct, fixed-size (ITCH/OUCH style)
- **TCP order entry** — port 9000
- **UDP multicast** — market data port 9001

### Simulasi & Bot
- **Tick-based simulator** — multi-bot
- **Random bot**, **Market Maker** (Avellaneda-Stoikov dasar)
- **Adaptive Market Maker** — spread widening berbasis VPIN
- **CSV logging** — snapshot per tick

### Riset
- **VPIN** — Volume-Synchronized Probability of Informed Trading
- **RL Market Making** — PPO/SAC notebook vs Avellaneda-Stoikov
- **Real Data** — yfinance integration for BTC/ETH price feed

---

## Benchmark (Real)

| Metric | Value |
|--------|-------|
| Ring buffer throughput (2-thread) | **503 M ops/sec** |
| Engine throughput | **8.5 M orders/sec** |
| Insert p99.9 latency | **32 μs** |
| Total tests | **56/56 PASS** |

---

## Arsitektur

```
trading-engine/
├── engine/                    # Core matching engine
│   ├── include/               # *.hpp (order, price_level, order_book, dll)
│   ├── src/                   # *.cpp implementations
│   └── tests/                 # 23 tests
├── network/                   # Network layer
│   ├── include/               # protocol, tcp_server, udp_feed
│   ├── src/                   # implementations + server_main
│   └── tests/                 # 12 tests
├── bots/                      # Simulasi & bot
│   ├── include/               # simulator, vpin
│   ├── src/                   # simulator.cpp
│   └── tests/                 # 12 tests
├── benchmarks/                # Google Benchmark
├── clients/                   # TCP/UDP client examples
├── scripts/                   # Research scripts (Python)
│   ├── rl_market_making.py    #  RL env + baseline
│   └── fetch_yfinance.py      #  BTC/ETH data downloader
├── research/
│   └── data/                  # CSV price data (gitignored)
└── CMakeLists.txt
```

---

## Build & Run

### Build
```bash
cd trading-engine
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
```

> **Windows note**: Butuh MinGW GCC 15.2.0+ di PATH (`C:\MinGW\bin`).  
> CMake bawaan MinGW (`C:\mingw64\bin\cmake`) corrupt — pake CMake portable  
> atau built-in dari CLion. Detail lengkap di `USAGE.md`.

### Test
```bash
build/test_matching.exe    # 6 tests
build/test_ring_buffer.exe # 6 tests
build/test_perpetual.exe   # 11 tests
build/test_simulation.exe  # 6 tests
build/test_vpin.exe        # 6 tests
build/test_protocol.exe    # 9 tests
build/test_tcp.exe         # 3 tests

# Semua test sekaligus
for f in build/test_*.exe; do echo "=== \$f ===" && "\$f"; done
```

### RL Research
```bash
# Baseline only (no deps)
python scripts/rl_market_making.py

# Full RL training
pip install stable-baselines3 gymnasium
python scripts/rl_market_making.py --train

# With real BTC price data
python scripts/fetch_yfinance.py --symbol BTC-USD --days 365
python scripts/rl_market_making.py --real-data research/data/BTC-USD_365d.csv
```

---

## Design Trade-offs

| Keputusan | Alternatif | Alasan |
|-----------|-----------|--------|
| `std::map` (BST) | `unordered_map` + sort manual | Butuh traversal terurut untuk price-time priority |
| `alignas(64)` | `hardware_destructive_interference_size` | C++17, manual lebih transparan |
| SPSC ring buffer | Lock-free queue | Cukup untuk 1 thread engine + 1 thread network |
| Binary protocol | JSON/Protobuf | 23 bytes vs 200+ bytes, zero parsing |
| Bot = callback | Class hierarchy | YAGNI, 2-3 bot pattern |
| VPIN header-only | .cpp terpisah | ~150 lines, compile sekali |
| Tick-based sim | Event-driven | Cukup untuk microstructure research |

---

## Roadmap

| Fase | Status |
|------|--------|
| 1 — Core Matching Engine | ✅ Selesai |
| 2 — Ring Buffer + Benchmark | ✅ Selesai |
| 3 — Advanced Order Types | ✅ Selesai |
| 4 — TCP/UDP Network Layer | ✅ Selesai |
| 5 — Mark Price & Funding Rate | ✅ Selesai |
| 6 — Bot Adversarial + Logging | ✅ Selesai |
| 7 — VPIN + Adaptive MM | ✅ Selesai |
| 8 — RL Market Making | ✅ Selesai (notebook) |
| 9 — Polish + Dokumentasi | ✅ Selesai |
| 10 — Buffer | ➖ Cadangan |

**Progress: ~96%** — sisanya opsional (dashboard, ADL).

---

## Catatan Riset RL

RL (PPO/SAC) untuk market making sering setara atau kalah dari Avellaneda-Stoikov tanpa reward shaping dan tuning ekstensif. Ini bukan kegagalan — temuan negatif dengan analisis tetap valid sebagai riset. Lihat proposal section 6 untuk detail.

---

## Lisensi

MIT
