// TCP Client — connects to the trading server and sends orders interactively.
// Usage: tcp_client [host] [port]
//
// Commands:
//   buy <price> <qty>              — limit buy
//   sell <price> <qty>             — limit sell
//   buy market <qty>               — market buy
//   sell market <qty>              — market sell
//   buy postonly <price> <qty>     — post-only buy
//   sell fok <price> <qty>         — fill-or-kill sell
//   buy ioc <price> <qty>          — immediate-or-cancel buy
//   buy iceberg <price> <qty> <visible>  — iceberg buy
//   cancel <order_id>              — cancel order
//   quit                           — exit

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "protocol.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")

static uint32_t next_id = 1;

static void printResponse(SOCKET sock) {
    uint8_t buf[64];
    int bytes = recv(sock, reinterpret_cast<char*>(buf), sizeof(buf), 0);
    if (bytes <= 0) {
        std::cout << "  [no response]" << std::endl;
        return;
    }

    size_t offset = 0;
    while (offset < static_cast<size_t>(bytes)) {
        uint8_t msg_type = buf[offset];
        if (msg_type == MsgType::EXECUTION) {
            ExecutionMsg msg;
            if (deserialize(buf + offset, bytes - offset, msg)) {
                std::cout << "  EXECUTION: order=" << msg.order_id
                          << " price=" << msg.fill_price
                          << " qty=" << msg.fill_qty
                          << " remaining=" << msg.remaining_qty << std::endl;
                offset += sizeof(ExecutionMsg);
            } else break;
        } else if (msg_type == MsgType::REJECT) {
            RejectMsg msg;
            if (deserialize(buf + offset, bytes - offset, msg)) {
                std::cout << "  REJECT: order=" << msg.order_id
                          << " reason=" << (int)msg.reason_code << std::endl;
                offset += sizeof(RejectMsg);
            } else break;
        } else if (msg_type == MsgType::ACCEPTED) {
            AcceptedMsg msg;
            if (deserialize(buf + offset, bytes - offset, msg)) {
                std::cout << "  ACCEPTED: order=" << msg.order_id
                          << " remaining=" << msg.remaining_qty << std::endl;
                offset += sizeof(AcceptedMsg);
            } else break;
        } else {
            std::cout << "  [unknown msg type 0x" << std::hex << (int)msg_type
                      << std::dec << "]" << std::endl;
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    uint16_t port = 9000;
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = static_cast<uint16_t>(std::atoi(argv[2]));

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "connect() failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << std::endl;
    std::cout << "Commands: buy/sell <price> <qty>, buy/sell market <qty>," << std::endl;
    std::cout << "          buy/sell postonly/fok/ioc <price> <qty>," << std::endl;
    std::cout << "          buy/sell iceberg <price> <qty> <visible>," << std::endl;
    std::cout << "          cancel <id>, quit" << std::endl;

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "quit" || line == "exit") break;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "cancel") {
            uint32_t oid;
            ss >> oid;
            CancelMsg msg;
            msg.msg_type = MsgType::CANCEL;
            msg.order_id = oid;
            send(sock, reinterpret_cast<const char*>(&msg), sizeof(msg), 0);
            printResponse(sock);
            continue;
        }

        // buy/sell [type] <price> <qty> [visible]
        uint8_t side = 0;
        if (cmd == "buy") side = 0;
        else if (cmd == "sell") side = 1;
        else {
            std::cout << "Unknown command: " << cmd << std::endl;
            continue;
        }

        std::string token;
        ss >> token;

        uint8_t order_type = OrderTypeCode::LIMIT;
        int64_t price = 0;
        uint32_t qty = 0;
        uint32_t visible = 0;

        if (token == "market") {
            order_type = OrderTypeCode::MARKET;
            ss >> qty;
            price = 0;
        } else if (token == "postonly") {
            order_type = OrderTypeCode::POSTONLY;
            ss >> price >> qty;
        } else if (token == "fok") {
            order_type = OrderTypeCode::FOK;
            ss >> price >> qty;
        } else if (token == "ioc") {
            order_type = OrderTypeCode::IOC;
            ss >> price >> qty;
        } else if (token == "iceberg") {
            order_type = OrderTypeCode::ICEBERG;
            ss >> price >> qty >> visible;
        } else {
            // Default: limit order — token is the price
            price = std::atoll(token.c_str());
            ss >> qty;
        }

        NewOrderMsg msg;
        msg.msg_type = MsgType::NEW_ORDER;
        msg.order_id = next_id++;
        msg.side = side;
        msg.order_type = order_type;
        msg.price = price;
        msg.quantity = qty;
        msg.visible_qty = visible;

        send(sock, reinterpret_cast<const char*>(&msg), sizeof(msg), 0);
        std::cout << "  Sent order #" << msg.order_id << std::endl;

        // Wait briefly for response
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        struct timeval tv = {1, 0};  // 1 second timeout
        if (select(0, &read_fds, nullptr, nullptr, &tv) > 0) {
            printResponse(sock);
        } else {
            std::cout << "  [timeout waiting for response]" << std::endl;
        }
    }

    closesocket(sock);
    WSACleanup();
    std::cout << "Disconnected." << std::endl;
    return 0;
}
