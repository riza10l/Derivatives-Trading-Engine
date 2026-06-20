#pragma once

// UDP Market Data Feed
// Broadcasts top-of-book, last trade, imbalance, and funding rate
// via UDP multicast (or unicast fallback for local testing).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "protocol.hpp"
#include "order_book.hpp"
#include "perpetual.hpp"
#include <string>
#include <cstdint>

class UdpMarketFeed {
public:
    UdpMarketFeed(const OrderBook& book, const PerpetualEngine& perp,
                  const std::string& multicast_group = "239.1.1.1",
                  uint16_t port = 9001);
    ~UdpMarketFeed();

    // Initialize the socket (call after WSAStartup)
    bool init();

    // Broadcast one market data snapshot
    void broadcast();

    // Get last sent message (for testing)
    const MarketDataMsg& lastMessage() const { return last_msg_; }

    uint16_t port() const { return port_; }

private:
    const OrderBook& book_;
    const PerpetualEngine& perp_;
    std::string group_;
    uint16_t port_;
    SOCKET sock_;
    struct sockaddr_in dest_addr_;
    MarketDataMsg last_msg_;
    bool initialized_;
};
