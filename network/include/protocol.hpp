#pragma once

// Binary protocol for the Derivatives Trading Engine.
// Inspired by NASDAQ ITCH/OUCH: fixed-size packed structs, no JSON overhead.
// All multi-byte fields are little-endian (native x86).

#include <cstdint>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
// Message type codes
// ---------------------------------------------------------------------------
namespace MsgType {
    // Client → Server
    constexpr uint8_t NEW_ORDER   = 0x01;
    constexpr uint8_t CANCEL      = 0x02;

    // Server → Client
    constexpr uint8_t EXECUTION   = 0x81;
    constexpr uint8_t REJECT      = 0x82;
    constexpr uint8_t ACCEPTED    = 0x83;

    // Server → All (UDP multicast)
    constexpr uint8_t MARKET_DATA = 0x91;
}

// ---------------------------------------------------------------------------
// Reject reason codes
// ---------------------------------------------------------------------------
namespace RejectReason {
    constexpr uint8_t POST_ONLY_CROSSED   = 0x01;
    constexpr uint8_t FOK_INSUFFICIENT    = 0x02;
    constexpr uint8_t UNKNOWN_ORDER       = 0x03;
    constexpr uint8_t INVALID_MESSAGE     = 0x04;
    constexpr uint8_t IOC_NO_FILL         = 0x05;
}

// ---------------------------------------------------------------------------
// Order type codes (matches OrderType enum ordinal)
// ---------------------------------------------------------------------------
namespace OrderTypeCode {
    constexpr uint8_t LIMIT   = 0x00;
    constexpr uint8_t MARKET  = 0x01;
    constexpr uint8_t POSTONLY = 0x02;
    constexpr uint8_t FOK     = 0x03;
    constexpr uint8_t IOC     = 0x04;
    constexpr uint8_t ICEBERG = 0x05;
}

// ---------------------------------------------------------------------------
// Packed message structs
// ---------------------------------------------------------------------------

#pragma pack(push, 1)

// Client → Server: new order
struct NewOrderMsg {
    uint8_t  msg_type;        // MsgType::NEW_ORDER (0x01)
    uint32_t order_id;        // client-assigned order id
    uint8_t  side;            // 0 = bid, 1 = ask
    uint8_t  order_type;      // OrderTypeCode
    int64_t  price;           // fixed-point price (ticks)
    uint32_t quantity;
    uint32_t visible_qty;     // 0 for non-iceberg
};
static_assert(sizeof(NewOrderMsg) == 23, "NewOrderMsg size mismatch");

// Client → Server: cancel
struct CancelMsg {
    uint8_t  msg_type;        // MsgType::CANCEL (0x02)
    uint32_t order_id;
};
static_assert(sizeof(CancelMsg) == 5, "CancelMsg size mismatch");

// Server → Client: execution report (one per fill)
struct ExecutionMsg {
    uint8_t  msg_type;        // MsgType::EXECUTION (0x81)
    uint32_t order_id;
    int64_t  fill_price;
    uint32_t fill_qty;
    uint32_t remaining_qty;
};
static_assert(sizeof(ExecutionMsg) == 21, "ExecutionMsg size mismatch");

// Server → Client: order rejected
struct RejectMsg {
    uint8_t  msg_type;        // MsgType::REJECT (0x82)
    uint32_t order_id;
    uint8_t  reason_code;
};
static_assert(sizeof(RejectMsg) == 6, "RejectMsg size mismatch");

// Server → Client: order accepted (resting on book)
struct AcceptedMsg {
    uint8_t  msg_type;        // MsgType::ACCEPTED (0x83)
    uint32_t order_id;
    uint32_t remaining_qty;
};
static_assert(sizeof(AcceptedMsg) == 9, "AcceptedMsg size mismatch");

// Server → All (UDP multicast): market data snapshot
struct MarketDataMsg {
    uint8_t  msg_type;        // MsgType::MARKET_DATA (0x91)
    int64_t  best_bid;
    int64_t  best_ask;
    int64_t  last_price;
    double   imbalance;
    double   funding_rate;
};
static_assert(sizeof(MarketDataMsg) == 41, "MarketDataMsg size mismatch");

#pragma pack(pop)

// ---------------------------------------------------------------------------
// Protocol helpers
// ---------------------------------------------------------------------------

// Get the expected payload size (excluding msg_type byte) for a given type.
// Returns 0 for unknown message types.
inline size_t messageSize(uint8_t msg_type) {
    switch (msg_type) {
        case MsgType::NEW_ORDER:   return sizeof(NewOrderMsg);
        case MsgType::CANCEL:      return sizeof(CancelMsg);
        case MsgType::EXECUTION:   return sizeof(ExecutionMsg);
        case MsgType::REJECT:      return sizeof(RejectMsg);
        case MsgType::ACCEPTED:    return sizeof(AcceptedMsg);
        case MsgType::MARKET_DATA: return sizeof(MarketDataMsg);
        default: return 0;
    }
}

// Serialize a struct into a byte buffer
template<typename T>
inline std::vector<uint8_t> serialize(const T& msg) {
    std::vector<uint8_t> buf(sizeof(T));
    std::memcpy(buf.data(), &msg, sizeof(T));
    return buf;
}

// Deserialize a byte buffer into a struct
template<typename T>
inline bool deserialize(const uint8_t* data, size_t len, T& out) {
    if (len < sizeof(T)) return false;
    std::memcpy(&out, data, sizeof(T));
    return true;
}
