//
// Created by rahul on 7/3/26.
//

#include <thread>

#include <gtest/gtest.h>
#include "lockfree/spsc_queue.h"

TEST(SPSCQueue, EmptyQueue) {
    spsc_queue<int, 8> q;

    int x;
    EXPECT_FALSE(q.pop(x));
}

TEST(SPSCQueue, PushPop) {
    spsc_queue<int, 8> q;

    EXPECT_TRUE(q.push(42));

    int x;
    EXPECT_TRUE(q.pop(x));
    EXPECT_EQ(x, 42);
}

TEST(SPSCQueue, FIFOOrder) {
    spsc_queue<int, 8> q;

    for (int i = 0; i < 5; ++i)
        EXPECT_TRUE(q.push(i));

    for (int i = 0; i < 5; ++i) {
        int x;
        EXPECT_TRUE(q.pop(x));
        EXPECT_EQ(x, i);
    }
}

TEST(SPSCQueue, EmptyAfterPop) {
    spsc_queue<int, 8> q;

    EXPECT_TRUE(q.push(10));

    int x;
    EXPECT_TRUE(q.pop(x));
    EXPECT_EQ(x, 10);

    EXPECT_FALSE(q.pop(x));
}

TEST(SPSCQueue, FullQueue) {
    spsc_queue<int, 8> q;

    // Capacity is effectively Capacity - 1.
    for (int i = 0; i < 7; ++i)
        EXPECT_TRUE(q.push(i));

    EXPECT_FALSE(q.push(100));
}

TEST(SPSCQueue, FillEmptyFill) {
    spsc_queue<int, 8> q;

    for (int round = 0; round < 10; ++round) {

        for (int i = 0; i < 7; ++i)
            EXPECT_TRUE(q.push(i));

        for (int i = 0; i < 7; ++i) {
            int x;
            EXPECT_TRUE(q.pop(x));
            EXPECT_EQ(x, i);
        }

        int x;
        EXPECT_FALSE(q.pop(x));
    }
}

TEST(SPSCQueue, WrapAround) {
    spsc_queue<int, 8> q;

    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(q.push(i));

        int x;
        EXPECT_TRUE(q.pop(x));
        EXPECT_EQ(x, i);
    }
}

TEST(SPSCQueue, Interleaved) {
    spsc_queue<int, 8> q;

    int x;

    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));

    EXPECT_TRUE(q.pop(x));
    EXPECT_EQ(x, 1);

    EXPECT_TRUE(q.push(3));

    EXPECT_TRUE(q.pop(x));
    EXPECT_EQ(x, 2);

    EXPECT_TRUE(q.pop(x));
    EXPECT_EQ(x, 3);

    EXPECT_FALSE(q.pop(x));
}

TEST(SPSCQueue, ManyCycles) {
    spsc_queue<int, 8> q;

    for (int i = 0; i < 10000; ++i) {
        EXPECT_TRUE(q.push(i));

        int x;
        EXPECT_TRUE(q.pop(x));
        EXPECT_EQ(x, i);
    }
}

TEST(SPSCQueue, ConcurrentProducerConsumer) {
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

            EXPECT_EQ(x, i);
        }
    });

    producer.join();
    consumer.join();
}