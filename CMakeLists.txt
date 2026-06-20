cmake_minimum_required(VERSION 3.16)
project(TradingEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine library (header-only so far)
add_library(engine INTERFACE)
target_include_directories(engine INTERFACE engine/include/ bots/include/)

# Tests
add_executable(test_matching engine/tests/test_matching.cpp engine/src/order_book.cpp engine/src/matching_engine.cpp)
target_link_libraries(test_matching PRIVATE engine)

add_executable(test_ring_buffer engine/tests/test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE engine)

add_executable(test_perpetual engine/tests/test_perpetual.cpp engine/src/perpetual.cpp engine/src/order_book.cpp engine/src/matching_engine.cpp)
target_link_libraries(test_perpetual PRIVATE engine)

# Simulation
add_executable(test_simulation bots/tests/test_simulation.cpp bots/src/simulator.cpp engine/src/order_book.cpp engine/src/matching_engine.cpp engine/src/perpetual.cpp)
target_link_libraries(test_simulation PRIVATE engine)

# Benchmark
add_executable(bench_matching benchmarks/bench_matching.cpp engine/src/order_book.cpp engine/src/matching_engine.cpp)
target_link_libraries(bench_matching PRIVATE engine)
