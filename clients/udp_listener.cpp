// UDP Market Data Listener — subscribes to multicast and prints market data.
// Usage: udp_listener [multicast_group] [port]

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "protocol.hpp"

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char* argv[]) {
    const char* group = "239.1.1.1";
    uint16_t port = 9001;
    if (argc >= 2) group = argv[1];
    if (argc >= 3) port = static_cast<uint16_t>(std::atoi(argv[2]));

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }

    // Allow multiple listeners on same port
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Bind to the multicast port
    struct sockaddr_in bind_addr;
    std::memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(port);

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Join multicast group
    struct ip_mreq mreq;
    inet_pton(AF_INET, group, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
        std::cerr << "Warning: multicast join failed (error " << WSAGetLastError()
                  << ") — falling back to unicast" << std::endl;
    }

    std::cout << "Listening for market data on " << group << ":" << port << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    std::cout << std::left
              << std::setw(12) << "Bid"
              << std::setw(12) << "Ask"
              << std::setw(12) << "Last"
              << std::setw(12) << "Imbalance"
              << std::setw(14) << "Funding"
              << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    uint8_t buf[128];
    while (true) {
        int bytes = recvfrom(sock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                             nullptr, nullptr);
        if (bytes <= 0) continue;

        if (buf[0] == MsgType::MARKET_DATA) {
            MarketDataMsg msg;
            if (deserialize(buf, bytes, msg)) {
                std::cout << std::left
                          << std::setw(12) << msg.best_bid
                          << std::setw(12) << msg.best_ask
                          << std::setw(12) << msg.last_price
                          << std::setw(12) << std::fixed << std::setprecision(4) << msg.imbalance
                          << std::setw(14) << std::setprecision(6) << msg.funding_rate
                          << std::endl;
            }
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
