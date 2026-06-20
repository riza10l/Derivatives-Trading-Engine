
cmake_minimum_required(VERSION 3.16)
project(TradingEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==========================================================================
# Engine static library
# ==========================================================================
add_library(engine_lib STATIC
    engine/src/order_book.cpp
    engine/src/matching_engine.cpp
    engine/src/perpetual.cpp
)
target_include_directories(engine_lib PUBLIC engine/include/ bots/include/ network/include/)

# ==========================================================================
# Network static library (links ws2_32 for WinSock2)
# ==========================================================================
add_library(network_lib STATIC
    network/src/tcp_server.cpp
    network/src/udp_feed.cpp
)
target_include_directories(network_lib PUBLIC network/include/)
target_link_libraries(network_lib PUBLIC engine_lib ws2_32)

# ==========================================================================
# Server executable
# ==========================================================================
add_executable(trading_server network/src/server_main.cpp)
target_link_libraries(trading_server PRIVATE network_lib)

# ==========================================================================
# Client executables
# ==========================================================================
add_executable(tcp_client clients/tcp_client.cpp)
target_link_libraries(tcp_client PRIVATE ws2_32)
target_include_directories(tcp_client PRIVATE network/include/)

add_executable(udp_listener clients/udp_listener.cpp)
target_link_libraries(udp_listener PRIVATE ws2_32)
target_include_directories(udp_listener PRIVATE network/include/)

# ==========================================================================
# Tests — Engine
# ==========================================================================
add_executable(test_matching engine/tests/test_matching.cpp)
target_link_libraries(test_matching PRIVATE engine_lib)

add_executable(test_ring_buffer engine/tests/test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE engine_lib)

add_executable(test_perpetual engine/tests/test_perpetual.cpp)
target_link_libraries(test_perpetual PRIVATE engine_lib)

# ==========================================================================
# Tests — Simulation
# ==========================================================================
add_executable(test_simulation bots/tests/test_simulation.cpp bots/src/simulator.cpp)
target_link_libraries(test_simulation PRIVATE engine_lib)

add_executable(test_vpin bots/tests/test_vpin.cpp bots/src/simulator.cpp)
target_link_libraries(test_vpin PRIVATE engine_lib)
target_include_directories(test_vpin PRIVATE bots/include/)

# ==========================================================================
# Tests — Network
# ==========================================================================
add_executable(test_protocol network/tests/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE network_lib)

add_executable(test_tcp network/tests/test_tcp.cpp)
target_link_libraries(test_tcp PRIVATE network_lib)

# ==========================================================================
# Benchmark
# ==========================================================================
add_executable(bench_matching benchmarks/bench_matching.cpp)
target_link_libraries(bench_matching PRIVATE engine_lib)
