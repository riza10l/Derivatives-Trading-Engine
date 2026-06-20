// Test: Binary protocol serialization/deserialization roundtrip

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

#include "protocol.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

void test_new_order_roundtrip() {
    std::cout << "Test: NewOrderMsg roundtrip... ";

    NewOrderMsg original;
    original.msg_type = MsgType::NEW_ORDER;
    original.order_id = 42;
    original.side = 0;  // bid
    original.order_type = OrderTypeCode::LIMIT;
    original.price = 10050;
    original.quantity = 100;
    original.visible_qty = 0;

    auto buf = serialize(original);
    assert(buf.size() == sizeof(NewOrderMsg));

    NewOrderMsg decoded;
    assert(deserialize(buf.data(), buf.size(), decoded));
    assert(decoded.msg_type == MsgType::NEW_ORDER);
    assert(decoded.order_id == 42);
    assert(decoded.side == 0);
    assert(decoded.order_type == OrderTypeCode::LIMIT);
    assert(decoded.price == 10050);
    assert(decoded.quantity == 100);
    assert(decoded.visible_qty == 0);

    std::cout << "PASS" << std::endl;
}

void test_cancel_roundtrip() {
    std::cout << "Test: CancelMsg roundtrip... ";

    CancelMsg original;
    original.msg_type = MsgType::CANCEL;
    original.order_id = 99;

    auto buf = serialize(original);
    assert(buf.size() == sizeof(CancelMsg));

    CancelMsg decoded;
    assert(deserialize(buf.data(), buf.size(), decoded));
    assert(decoded.msg_type == MsgType::CANCEL);
    assert(decoded.order_id == 99);

    std::cout << "PASS" << std::endl;
}

void test_execution_roundtrip() {
    std::cout << "Test: ExecutionMsg roundtrip... ";

    ExecutionMsg original;
    original.msg_type = MsgType::EXECUTION;
    original.order_id = 7;
    original.fill_price = 10100;
    original.fill_qty = 25;
    original.remaining_qty = 75;

    auto buf = serialize(original);
    ExecutionMsg decoded;
    assert(deserialize(buf.data(), buf.size(), decoded));
    assert(decoded.order_id == 7);
    assert(decoded.fill_price == 10100);
    assert(decoded.fill_qty == 25);
    assert(decoded.remaining_qty == 75);

    std::cout << "PASS" << std::endl;
}

void test_reject_roundtrip() {
    std::cout << "Test: RejectMsg roundtrip... ";

    RejectMsg original;
    original.msg_type = MsgType::REJECT;
    original.order_id = 13;
    original.reason_code = RejectReason::POST_ONLY_CROSSED;

    auto buf = serialize(original);
    RejectMsg decoded;
    assert(deserialize(buf.data(), buf.size(), decoded));
    assert(decoded.order_id == 13);
    assert(decoded.reason_code == RejectReason::POST_ONLY_CROSSED);

    std::cout << "PASS" << std::endl;
}

void test_accepted_roundtrip() {
    std::cout << "Test: AcceptedMsg roundtrip... ";

    AcceptedMsg original;
    original.msg_type = MsgType::ACCEPTED;
    original.order_id = 55;
    original.remaining_qty = 200;

    auto buf = serialize(original);
    AcceptedMsg decoded;
    assert(deserialize(buf.data(), buf.size(), decoded));
    assert(decoded.order_id == 55);
    assert(decoded.remaining_qty == 200);

    std::cout << "PASS" << std::endl;
}

void test_market_data_roundtrip() {
    std::cout << "Test: MarketDataMsg roundtrip... ";

    MarketDataMsg original;
    original.msg_type = MsgType::MARKET_DATA;
    original.best_bid = 10000;
    original.best_ask = 10050;
    original.last_price = 10025;
    original.imbalance = 0.1234;
    original.funding_rate = -0.0005;

    auto buf = serialize(original);
    assert(buf.size() == sizeof(MarketDataMsg));

    MarketDataMsg decoded;
    assert(deserialize(buf.data(), buf.size(), decoded));
    assert(decoded.best_bid == 10000);
    assert(decoded.best_ask == 10050);
    assert(decoded.last_price == 10025);
    // Floating point comparison with tolerance
    assert(decoded.imbalance > 0.1233 && decoded.imbalance < 0.1235);
    assert(decoded.funding_rate > -0.0006 && decoded.funding_rate < -0.0004);

    std::cout << "PASS" << std::endl;
}

void test_struct_sizes() {
    std::cout << "Test: Packed struct sizes... ";

    assert(sizeof(NewOrderMsg) == 23);
    assert(sizeof(CancelMsg) == 5);
    assert(sizeof(ExecutionMsg) == 21);
    assert(sizeof(RejectMsg) == 6);
    assert(sizeof(AcceptedMsg) == 9);
    assert(sizeof(MarketDataMsg) == 41);

    std::cout << "PASS" << std::endl;
    std::cout << "  NewOrderMsg:   " << sizeof(NewOrderMsg) << " bytes" << std::endl;
    std::cout << "  CancelMsg:     " << sizeof(CancelMsg) << " bytes" << std::endl;
    std::cout << "  ExecutionMsg:  " << sizeof(ExecutionMsg) << " bytes" << std::endl;
    std::cout << "  RejectMsg:     " << sizeof(RejectMsg) << " bytes" << std::endl;
    std::cout << "  AcceptedMsg:   " << sizeof(AcceptedMsg) << " bytes" << std::endl;
    std::cout << "  MarketDataMsg: " << sizeof(MarketDataMsg) << " bytes" << std::endl;
}

void test_message_size_lookup() {
    std::cout << "Test: messageSize() lookup... ";

    assert(messageSize(MsgType::NEW_ORDER) == sizeof(NewOrderMsg));
    assert(messageSize(MsgType::CANCEL) == sizeof(CancelMsg));
    assert(messageSize(MsgType::EXECUTION) == sizeof(ExecutionMsg));
    assert(messageSize(MsgType::REJECT) == sizeof(RejectMsg));
    assert(messageSize(MsgType::ACCEPTED) == sizeof(AcceptedMsg));
    assert(messageSize(MsgType::MARKET_DATA) == sizeof(MarketDataMsg));
    assert(messageSize(0xFF) == 0);  // unknown type

    std::cout << "PASS" << std::endl;
}

void test_deserialize_too_short() {
    std::cout << "Test: Deserialize with insufficient buffer... ";

    uint8_t buf[2] = {MsgType::NEW_ORDER, 0x00};
    NewOrderMsg msg;
    assert(!deserialize(buf, 2, msg));  // too short

    std::cout << "PASS" << std::endl;
}

void test_iceberg_order_msg() {
    std::cout << "Test: Iceberg NewOrderMsg... ";

    NewOrderMsg msg;
    msg.msg_type = MsgType::NEW_ORDER;
    msg.order_id = 100;
    msg.side = 0;
    msg.order_type = OrderTypeCode::ICEBERG;
    msg.price = 5000;
    msg.quantity = 50;
    msg.visible_qty = 10;

    auto buf = serialize(msg);
    NewOrderMsg decoded;
    assert(deserialize(buf.data(), buf.size(), decoded));
    assert(decoded.order_type == OrderTypeCode::ICEBERG);
    assert(decoded.quantity == 50);
    assert(decoded.visible_qty == 10);

    std::cout << "PASS" << std::endl;
}

int main() {
    std::cout << "=== Protocol Tests ===" << std::endl;
    test_struct_sizes();
    test_new_order_roundtrip();
    test_cancel_roundtrip();
    test_execution_roundtrip();
    test_reject_roundtrip();
    test_accepted_roundtrip();
    test_market_data_roundtrip();
    test_message_size_lookup();
    test_deserialize_too_short();
    test_iceberg_order_msg();
    std::cout << "\nAll protocol tests passed!" << std::endl;
    return 0;
}
