//
// Created by rahul on 7/3/26.
//

#include <cassert>
#include <iostream>
#include <thread>

#include "lockfree/spsc_queue.h"

void test_empty_queue() {
    spsc_queue<int, 8> q;

    int x;
    assert(!q.pop(x));
}

void test_push_pop() {
    spsc_queue<int, 8> q;

    assert(q.push(42));

    int x;
    assert(q.pop(x));
    assert(x == 42);
}

void test_fifo_order() {
    spsc_queue<int, 8> q;

    for (int i = 0; i < 5; ++i)
        assert(q.push(i));

    for (int i = 0; i < 5; ++i) {
        int x;
        assert(q.pop(x));
        assert(x == i);
    }
}

void test_empty_after_pop() {
    spsc_queue<int, 8> q;

    assert(q.push(10));

    int x;
    assert(q.pop(x));
    assert(x == 10);

    assert(!q.pop(x));
}

void test_full_queue() {
    spsc_queue<int, 8> q;

    // Assuming one slot is left empty to distinguish full from empty.
    for (int i = 0; i < 7; ++i)
        assert(q.push(i));

    assert(!q.push(100));
}

void test_fill_empty_fill() {
    spsc_queue<int, 8> q;

    for (int round = 0; round < 10; ++round) {

        for (int i = 0; i < 7; ++i)
            assert(q.push(i));

        for (int i = 0; i < 7; ++i) {
            int x;
            assert(q.pop(x));
            assert(x == i);
        }

        int x;
        assert(!q.pop(x));
    }
}

void test_wraparound() {
    spsc_queue<int, 8> q;

    for (int i = 0; i < 1000; ++i) {
        assert(q.push(i));

        int x;
        assert(q.pop(x));
        assert(x == i);
    }
}

void test_interleaved() {
    spsc_queue<int, 8> q;

    int x;

    assert(q.push(1));
    assert(q.push(2));

    assert(q.pop(x));
    assert(x == 1);

    assert(q.push(3));

    assert(q.pop(x));
    assert(x == 2);

    assert(q.pop(x));
    assert(x == 3);

    assert(!q.pop(x));
}

void test_many_cycles() {
    spsc_queue<int, 8> q;

    for (int i = 0; i < 10000; ++i) {
        assert(q.push(i));

        int x;
        assert(q.pop(x));
        assert(x == i);
    }
}

void test_spsc_concurrent() {
    constexpr int N = 1'000'000;

    spsc_queue<int, 1024> q;

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!q.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < N; ++i) {
            int x;

            while (!q.pop(x)) {
                std::this_thread::yield();
            }

            assert(x == i);
        }
    });

    producer.join();
    consumer.join();
}

template <typename TestFunc>
void run_test(const char* name, TestFunc test) {
    std::cout << "[ RUN      ] " << name << '\n';
    test();
    std::cout << "[       OK ] " << name << "\n";
}

int main() {
    run_test("EmptyQueue", test_empty_queue);
    run_test("PushPop", test_push_pop);
    run_test("FIFOOrder", test_fifo_order);
    run_test("EmptyAfterPop", test_empty_after_pop);
    run_test("FullQueue", test_full_queue);
    run_test("FillEmptyFill", test_fill_empty_fill);
    run_test("WrapAround", test_wraparound);
    run_test("Interleaved", test_interleaved);
    run_test("ManyCycles", test_many_cycles);

    run_test("ConcurrentProducerConsumer", test_spsc_concurrent);

    std::cout << "\n=====================================\n";
    std::cout << "All tests passed successfully!\n";
    std::cout << "=====================================\n";

    return 0;
}