#include "tcp_server.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

// Link with ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

// Map OrderTypeCode → OrderType enum
static OrderType codeToOrderType(uint8_t code) {
    switch (code) {
        case OrderTypeCode::LIMIT:   return OrderType::Limit;
        case OrderTypeCode::MARKET:  return OrderType::Market;
        case OrderTypeCode::POSTONLY: return OrderType::PostOnly;
        case OrderTypeCode::FOK:     return OrderType::FOK;
        case OrderTypeCode::IOC:     return OrderType::IOC;
        case OrderTypeCode::ICEBERG: return OrderType::Iceberg;
        default:                     return OrderType::Limit;
    }
}

TcpServer::TcpServer(MatchingEngine& engine, uint16_t port)
    : engine_(engine), port_(port), listen_fd_(INVALID_SOCKET), running_(false) {}

TcpServer::~TcpServer() {
    stop();
    for (auto fd : clients_) {
        closesocket(fd);
    }
    if (listen_fd_ != INVALID_SOCKET) {
        closesocket(listen_fd_);
    }
    WSACleanup();
}

bool TcpServer::initWinsock() {
    WSADATA wsa;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (result != 0) {
        std::cerr << "[tcp] WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}

bool TcpServer::bindAndListen() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd_ == INVALID_SOCKET) {
        std::cerr << "[tcp] socket() failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Set non-blocking
    u_long mode = 1;
    ioctlsocket(listen_fd_, FIONBIO, &mode);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[tcp] bind() failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[tcp] listen() failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
        return false;
    }

    std::cout << "[tcp] Listening on port " << port_ << std::endl;
    return true;
}

void TcpServer::start() {
    if (!initWinsock()) return;
    if (!bindAndListen()) return;

    running_ = true;
    acceptLoop();
}

void TcpServer::stop() {
    running_ = false;
}

void TcpServer::acceptLoop() {
    while (running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd_, &read_fds);

        for (auto fd : clients_) {
            FD_SET(fd, &read_fds);
        }

        // 100ms timeout so we can check running_ flag
        // pake 100 ms biar gak kelamaan
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms

        int ready = select(0, &read_fds, nullptr, nullptr, &tv);
        if (ready == SOCKET_ERROR) {
            std::cerr << "[tcp] select() error: " << WSAGetLastError() << std::endl;
            break;
        }

        if (ready == 0) continue;  // timeout

        // Check for new connections
        if (FD_ISSET(listen_fd_, &read_fds)) {
            struct sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            SOCKET client_fd = accept(listen_fd_,
                reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
            if (client_fd != INVALID_SOCKET) {
                // Set non-blocking
                u_long mode = 1;
                ioctlsocket(client_fd, FIONBIO, &mode);
                clients_.push_back(client_fd);
                std::cout << "[tcp] Client connected (fd=" << client_fd
                          << ", total=" << clients_.size() << ")" << std::endl;
            }
        }

        // Check existing clients for data
        // Copy client list since handleClientData may modify it via removeClient
        auto clients_copy = clients_;
        for (auto fd : clients_copy) {
            if (FD_ISSET(fd, &read_fds)) {
                handleClientData(fd);
            }
        }
    }
}

void TcpServer::handleClientData(SOCKET fd) {
    uint8_t buf[256];
    int bytes = recv(fd, reinterpret_cast<char*>(buf), sizeof(buf), 0);

    if (bytes <= 0) {
        if (bytes == 0) {
            std::cout << "[tcp] Client disconnected (fd=" << fd << ")" << std::endl;
        } else {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                std::cerr << "[tcp] recv error: " << err << std::endl;
            } else {
                return;  // no data yet
            }
        }
        removeClient(fd);
        return;
    }

    // Process messages in the buffer
    // Simple approach: one message per read (OK for low-throughput testing)
    // In production: use a per-client ring buffer for framing
    size_t offset = 0;
    while (offset < static_cast<size_t>(bytes)) {
        size_t remaining = bytes - offset;
        if (remaining < 1) break;

        uint8_t msg_type = buf[offset];
        size_t msg_size = messageSize(msg_type);
        if (msg_size == 0 || remaining < msg_size) {
            // Invalid or incomplete message
            RejectMsg rej;
            rej.msg_type = MsgType::REJECT;
            rej.order_id = 0;
            rej.reason_code = RejectReason::INVALID_MESSAGE;
            sendToClient(fd, &rej, sizeof(rej));
            break;
        }

        processMessage(fd, buf + offset, msg_size);
        offset += msg_size;
    }
}

void TcpServer::processMessage(SOCKET fd, const uint8_t* data, size_t len) {
    uint8_t msg_type = data[0];

    if (msg_type == MsgType::NEW_ORDER) {
        NewOrderMsg msg;
        if (!deserialize(data, len, msg)) return;

        Side side = (msg.side == 0) ? Side::Bid : Side::Ask;
        OrderType type = codeToOrderType(msg.order_type);

        auto result = engine_.submitOrder(side, type, msg.price, msg.quantity,
                                           msg.visible_qty);

        // Send execution reports for each fill
        for (const auto& trade : result.trades) {
            ExecutionMsg exec;
            exec.msg_type = MsgType::EXECUTION;
            exec.order_id = static_cast<uint32_t>(result.id);
            exec.fill_price = trade.price;
            exec.fill_qty = trade.qty;
            exec.remaining_qty = 0;  // filled for this trade

            // Compute actual remaining after all trades
            Quantity total_filled = 0;
            for (const auto& t : result.trades) total_filled += t.qty;
            exec.remaining_qty = (msg.quantity > total_filled)
                                 ? msg.quantity - total_filled : 0;

            sendToClient(fd, &exec, sizeof(exec));
        }

        // Send status message based on result
        if (result.status == SubmitStatus::Rejected) {
            RejectMsg rej;
            rej.msg_type = MsgType::REJECT;
            rej.order_id = static_cast<uint32_t>(result.id);
            rej.reason_code = RejectReason::POST_ONLY_CROSSED;
            sendToClient(fd, &rej, sizeof(rej));
        } else if (result.status == SubmitStatus::Cancelled) {
            RejectMsg rej;
            rej.msg_type = MsgType::REJECT;
            rej.order_id = static_cast<uint32_t>(result.id);
            rej.reason_code = RejectReason::FOK_INSUFFICIENT;
            sendToClient(fd, &rej, sizeof(rej));
        } else if (result.status == SubmitStatus::Accepted && result.trades.empty()) {
            AcceptedMsg acc;
            acc.msg_type = MsgType::ACCEPTED;
            acc.order_id = static_cast<uint32_t>(result.id);
            acc.remaining_qty = msg.quantity;
            sendToClient(fd, &acc, sizeof(acc));
        }

        // Trigger post-trade callback (e.g., broadcast market data)
        if (post_trade_cb_ && !result.trades.empty()) {
            post_trade_cb_();
        }

    } else if (msg_type == MsgType::CANCEL) {
        CancelMsg msg;
        if (!deserialize(data, len, msg)) return;

        bool ok = engine_.cancelOrder(static_cast<OrderId>(msg.order_id));
        if (ok) {
            AcceptedMsg acc;
            acc.msg_type = MsgType::ACCEPTED;
            acc.order_id = msg.order_id;
            acc.remaining_qty = 0;
            sendToClient(fd, &acc, sizeof(acc));
        } else {
            RejectMsg rej;
            rej.msg_type = MsgType::REJECT;
            rej.order_id = msg.order_id;
            rej.reason_code = RejectReason::UNKNOWN_ORDER;
            sendToClient(fd, &rej, sizeof(rej));
        }
    }
}

void TcpServer::sendToClient(SOCKET fd, const void* data, size_t len) {
    send(fd, reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
}

void TcpServer::removeClient(SOCKET fd) {
    closesocket(fd);
    clients_.erase(std::remove(clients_.begin(), clients_.end(), fd), clients_.end());
}
