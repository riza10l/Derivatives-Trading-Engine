// Test: TCP server end-to-end
// Spins up a server in a thread, connects a client, sends orders, verifies responses.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "tcp_server.hpp"
#include "protocol.hpp"
#include "matching_engine.hpp"

#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

static const uint16_t TEST_PORT = 19000;  // use high port to avoid conflicts

// Helper: connect a TCP client socket
static SOCKET connectClient() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    // Retry connection a few times (server may not be ready yet)
    for (int i = 0; i < 20; ++i) {
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            return sock;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    closesocket(sock);
    return INVALID_SOCKET;
}

// Helper: receive a message with timeout
static int recvWithTimeout(SOCKET sock, uint8_t* buf, int buflen, int timeout_ms = 2000) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (select(0, &fds, nullptr, nullptr, &tv) <= 0) return 0;
    return recv(sock, reinterpret_cast<char*>(buf), buflen, 0);
}

void test_tcp_limit_order_and_match() {
    std::cout << "Test: TCP limit order + match... ";

    MatchingEngine engine;
    TcpServer server(engine, TEST_PORT);

    std::thread server_thread([&]() { server.start(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    SOCKET client = connectClient();
    assert(client != INVALID_SOCKET);

    // Send a resting bid
    NewOrderMsg bid;
    bid.msg_type = MsgType::NEW_ORDER;
    bid.order_id = 1;
    bid.side = 0;  // bid
    bid.order_type = OrderTypeCode::LIMIT;
    bid.price = 100;
    bid.quantity = 10;
    bid.visible_qty = 0;
    send(client, reinterpret_cast<const char*>(&bid), sizeof(bid), 0);

    // Should get ACCEPTED back
    uint8_t buf[64];
    int bytes = recvWithTimeout(client, buf, sizeof(buf));
    assert(bytes > 0);
    assert(buf[0] == MsgType::ACCEPTED);

    // Send a crossing ask → should get EXECUTION
    NewOrderMsg ask;
    ask.msg_type = MsgType::NEW_ORDER;
    ask.order_id = 2;
    ask.side = 1;  // ask
    ask.order_type = OrderTypeCode::LIMIT;
    ask.price = 100;
    ask.quantity = 5;
    ask.visible_qty = 0;
    send(client, reinterpret_cast<const char*>(&ask), sizeof(ask), 0);

    bytes = recvWithTimeout(client, buf, sizeof(buf));
    assert(bytes > 0);
    assert(buf[0] == MsgType::EXECUTION);

    ExecutionMsg exec;
    assert(deserialize(buf, bytes, exec));
    assert(exec.fill_price == 100);
    assert(exec.fill_qty == 5);

    closesocket(client);
    server.stop();
    server_thread.join();

    std::cout << "PASS" << std::endl;
}

void test_tcp_post_only_reject() {
    std::cout << "Test: TCP Post-Only rejection... ";

    MatchingEngine engine;
    TcpServer server(engine, TEST_PORT + 1);

    std::thread server_thread([&]() { server.start(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    SOCKET client = connectClient();
    // Re-connect to the right port
    closesocket(client);
    {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_PORT + 1);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        for (int i = 0; i < 20; ++i) {
            if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        client = sock;
    }
    assert(client != INVALID_SOCKET);

    // First: place a resting bid
    NewOrderMsg bid;
    bid.msg_type = MsgType::NEW_ORDER;
    bid.order_id = 1;
    bid.side = 0;
    bid.order_type = OrderTypeCode::LIMIT;
    bid.price = 100;
    bid.quantity = 10;
    bid.visible_qty = 0;
    send(client, reinterpret_cast<const char*>(&bid), sizeof(bid), 0);

    uint8_t buf[64];
    recvWithTimeout(client, buf, sizeof(buf));  // consume ACCEPTED

    // Now: send post-only ask at 100 → should be REJECTED
    NewOrderMsg post_only;
    post_only.msg_type = MsgType::NEW_ORDER;
    post_only.order_id = 2;
    post_only.side = 1;
    post_only.order_type = OrderTypeCode::POSTONLY;
    post_only.price = 100;
    post_only.quantity = 5;
    post_only.visible_qty = 0;
    send(client, reinterpret_cast<const char*>(&post_only), sizeof(post_only), 0);

    int bytes = recvWithTimeout(client, buf, sizeof(buf));
    assert(bytes > 0);
    assert(buf[0] == MsgType::REJECT);

    RejectMsg rej;
    assert(deserialize(buf, bytes, rej));
    assert(rej.reason_code == RejectReason::POST_ONLY_CROSSED);

    closesocket(client);
    server.stop();
    server_thread.join();

    std::cout << "PASS" << std::endl;
}

void test_tcp_cancel_order() {
    std::cout << "Test: TCP cancel order... ";

    MatchingEngine engine;
    TcpServer server(engine, TEST_PORT + 2);

    std::thread server_thread([&]() { server.start(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    SOCKET client = INVALID_SOCKET;
    {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_PORT + 2);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        for (int i = 0; i < 20; ++i) {
            if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        client = sock;
    }
    assert(client != INVALID_SOCKET);

    // Place an order
    NewOrderMsg bid;
    bid.msg_type = MsgType::NEW_ORDER;
    bid.order_id = 1;
    bid.side = 0;
    bid.order_type = OrderTypeCode::LIMIT;
    bid.price = 100;
    bid.quantity = 10;
    bid.visible_qty = 0;
    send(client, reinterpret_cast<const char*>(&bid), sizeof(bid), 0);

    uint8_t buf[64];
    recvWithTimeout(client, buf, sizeof(buf));  // consume ACCEPTED

    // Cancel non-existent order → REJECT
    CancelMsg cancel;
    cancel.msg_type = MsgType::CANCEL;
    cancel.order_id = 999;  // doesn't exist
    send(client, reinterpret_cast<const char*>(&cancel), sizeof(cancel), 0);

    int bytes = recvWithTimeout(client, buf, sizeof(buf));
    assert(bytes > 0);
    assert(buf[0] == MsgType::REJECT);

    // Cancel existing order (engine assigns id=1 for first order)
    cancel.order_id = 1;
    send(client, reinterpret_cast<const char*>(&cancel), sizeof(cancel), 0);

    bytes = recvWithTimeout(client, buf, sizeof(buf));
    assert(bytes > 0);
    assert(buf[0] == MsgType::ACCEPTED);

    closesocket(client);
    server.stop();
    server_thread.join();

    std::cout << "PASS" << std::endl;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::cout << "=== TCP Server Tests ===" << std::endl;
    test_tcp_limit_order_and_match();
    test_tcp_post_only_reject();
    test_tcp_cancel_order();
    std::cout << "\nAll TCP tests passed!" << std::endl;

    WSACleanup();
    return 0;
}
