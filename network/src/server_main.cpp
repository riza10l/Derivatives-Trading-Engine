// Derivatives Trading Engine — Server Main
// Starts TCP order entry (port 9000) and UDP market data feed (port 9001).
// Usage: trading_server [tcp_port] [udp_port]

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "tcp_server.hpp"
#include "udp_feed.hpp"
#include "matching_engine.hpp"
#include "perpetual.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")

static std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    uint16_t tcp_port = 9000;
    uint16_t udp_port = 9001;

    if (argc >= 2) tcp_port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (argc >= 3) udp_port = static_cast<uint16_t>(std::atoi(argv[2]));

    std::signal(SIGINT, signalHandler);

    std::cout << "========================================" << std::endl;
    std::cout << " Derivatives Trading Engine Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << " TCP Order Entry : port " << tcp_port << std::endl;
    std::cout << " UDP Market Data : port " << udp_port << std::endl;
    std::cout << "========================================" << std::endl;

    // --- Core engine ---
    MatchingEngine engine;
    PerpetualEngine perpetual(&engine.book());

    // --- UDP market data feed ---
    // Note: WSAStartup is called by TcpServer; we share the same WSA context.
    // So we init UDP after TCP starts. For standalone, call WSAStartup here.
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    UdpMarketFeed udp_feed(engine.book(), perpetual, "239.1.1.1", udp_port);
    udp_feed.init();

    // --- UDP broadcast thread ---
    std::thread udp_thread([&]() {
        while (g_running.load()) {
            perpetual.tick();
            udp_feed.broadcast();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // --- TCP server (blocking on main thread) ---
    TcpServer tcp_server(engine, tcp_port);

    // Hook: broadcast market data after each trade
    tcp_server.setPostTradeCallback([&]() {
        udp_feed.broadcast();
    });

    // Run TCP event loop in a thread so we can handle shutdown
    std::thread tcp_thread([&]() {
        tcp_server.start();
    });

    // Wait for CTRL+C
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "\n[server] Shutting down..." << std::endl;

    tcp_server.stop();
    if (tcp_thread.joinable()) tcp_thread.join();
    if (udp_thread.joinable()) udp_thread.join();

    WSACleanup();

    std::cout << "[server] Total trades: " << engine.totalTrades() << std::endl;
    std::cout << "[server] Goodbye." << std::endl;
    return 0;
}
