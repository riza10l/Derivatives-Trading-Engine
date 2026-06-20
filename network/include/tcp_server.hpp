#pragma once

// TCP Order Entry Server
// Single-threaded select()-based event loop — no mutex needed since
// MatchingEngine is single-threaded.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "protocol.hpp"
#include "matching_engine.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <functional>

class TcpServer {
public:
    // Construct with reference to the engine and optional port.
    TcpServer(MatchingEngine& engine, uint16_t port = 9000);
    ~TcpServer();

    // Start the event loop (blocking).  Returns when stop() is called.
    void start();

    // Signal the event loop to stop (thread-safe).
    void stop();

    // Callback invoked after each order submission (optional).
    // Useful for triggering market data broadcasts.
    using PostTradeCallback = std::function<void()>;
    void setPostTradeCallback(PostTradeCallback cb) { post_trade_cb_ = std::move(cb); }

    uint16_t port() const { return port_; }

private:
    MatchingEngine& engine_;
    uint16_t port_;
    SOCKET listen_fd_;
    std::vector<SOCKET> clients_;
    volatile bool running_;
    PostTradeCallback post_trade_cb_;

    // Internal
    bool initWinsock();
    bool bindAndListen();
    void acceptLoop();
    void handleClientData(SOCKET fd);
    void processMessage(SOCKET fd, const uint8_t* data, size_t len);
    void sendToClient(SOCKET fd, const void* data, size_t len);
    void removeClient(SOCKET fd);
};
