#include "ring_buffer.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

void test_basic_push_pop() {
    std::cout << "Test: Basic push/pop... ";
    SPSCRingBuffer<int, 4> rb;

    assert(rb.empty());
    assert(!rb.full());

    assert(rb.try_push(1));
    assert(rb.try_push(2));
    assert(rb.size() == 2);

    int val;
    assert(rb.try_pop(val) && val == 1);
    assert(rb.try_pop(val) && val == 2);
    assert(rb.empty());
    std::cout << "PASS" << std::endl;
}

void test_full_buffer() {
    std::cout << "Test: Full buffer... ";
    SPSCRingBuffer<int, 4> rb;

    assert(rb.try_push(1));
    assert(rb.try_push(2));
    assert(rb.try_push(3));
    assert(rb.try_push(4));
    assert(rb.full());
    assert(!rb.try_push(5)); // should fail

    int val;
    assert(rb.try_pop(val) && val == 1);
    assert(!rb.full());
    assert(rb.try_push(5)); // should succeed now
    assert(rb.size() == 4);

    // Drain
    assert(rb.try_pop(val) && val == 2);
    assert(rb.try_pop(val) && val == 3);
    assert(rb.try_pop(val) && val == 4);
    assert(rb.try_pop(val) && val == 5);
    assert(rb.empty());
    std::cout << "PASS" << std::endl;
}

void test_wraparound() {
    std::cout << "Test: Sequence wrap-around... ";
    SPSCRingBuffer<int, 4> rb;
    int val;

    // Write enough to wrap sequence counter past capacity
    for (int i = 0; i < 100; ++i) {
        assert(rb.try_push(i));
        assert(rb.try_pop(val) && val == i);
    }
    assert(rb.empty());
    std::cout << "PASS" << std::endl;
}

void test_multiple_cycles() {
    std::cout << "Test: Multiple fill/drain cycles... ";
    SPSCRingBuffer<int, 4> rb;
    int val;

    for (int cycle = 0; cycle < 10; ++cycle) {
        for (int i = 0; i < 4; ++i) {
            assert(rb.try_push(cycle * 10 + i));
        }
        assert(rb.full());
        for (int i = 0; i < 4; ++i) {
            assert(rb.try_pop(val) && val == cycle * 10 + i);
        }
        assert(rb.empty());
    }
    std::cout << "PASS" << std::endl;
}

void test_push_pop_order() {
    std::cout << "Test: Push/pop with Order struct... ";
    SPSCRingBuffer<int, 1024> rb;
    int val;

    for (int i = 0; i < 1024; ++i) {
        assert(rb.try_push(i));
    }
    assert(rb.full());

    for (int i = 0; i < 1024; ++i) {
        assert(rb.try_pop(val) && val == i);
    }
    assert(rb.empty());
    std::cout << "PASS" << std::endl;
}

void test_spin_push_pop() {
    std::cout << "Test: Spin-based push/pop... ";
    SPSCRingBuffer<int, 1024> rb;

    for (int i = 0; i < 1024; ++i) {
        rb.push(i);
    }
    assert(rb.full());

    for (int i = 0; i < 1024; ++i) {
        int val = rb.pop();
        assert(val == i);
    }
    assert(rb.empty());
    std::cout << "PASS" << std::endl;
}

int main() {
    std::cout << "=== Ring Buffer Tests ===" << std::endl;
    test_basic_push_pop();
    test_full_buffer();
    test_wraparound();
    test_multiple_cycles();
    test_push_pop_order();
    test_spin_push_pop();
    std::cout << "\nAll ring buffer tests passed!" << std::endl;
    return 0;
}
