#include "udp_feed.hpp"
#include <iostream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

UdpMarketFeed::UdpMarketFeed(const OrderBook& book, const PerpetualEngine& perp,
                               const std::string& multicast_group, uint16_t port)
    : book_(book), perp_(perp), group_(multicast_group), port_(port),
      sock_(INVALID_SOCKET), initialized_(false) {
    std::memset(&last_msg_, 0, sizeof(last_msg_));
    std::memset(&dest_addr_, 0, sizeof(dest_addr_));
}

UdpMarketFeed::~UdpMarketFeed() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
    }
}

bool UdpMarketFeed::init() {
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == INVALID_SOCKET) {
        std::cerr << "[udp] socket() failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    // Set multicast TTL
    int ttl = 1;
    setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL,
               reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    // Allow loopback (for local testing)
    int loop = 1;
    setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_LOOP,
               reinterpret_cast<const char*>(&loop), sizeof(loop));

    // Set destination address
    dest_addr_.sin_family = AF_INET;
    dest_addr_.sin_port = htons(port_);
    inet_pton(AF_INET, group_.c_str(), &dest_addr_.sin_addr);

    initialized_ = true;
    std::cout << "[udp] Market data feed ready → " << group_ << ":" << port_ << std::endl;
    return true;
}

void UdpMarketFeed::broadcast() {
    if (!initialized_) return;

    MarketDataMsg msg;
    msg.msg_type = MsgType::MARKET_DATA;
    msg.best_bid = book_.bestBid().value_or(0);
    msg.best_ask = book_.bestAsk().value_or(0);
    msg.last_price = perp_.lastTradePrice();
    msg.imbalance = book_.imbalance();
    msg.funding_rate = perp_.fundingRate();

    last_msg_ = msg;

    int sent = sendto(sock_, reinterpret_cast<const char*>(&msg), sizeof(msg), 0,
                      reinterpret_cast<struct sockaddr*>(&dest_addr_), sizeof(dest_addr_));
    if (sent == SOCKET_ERROR) {
        // Silently fail — multicast might not be available
        // This is OK for local development
    }
}
