//
// Created by rahul on 7/17/26.
//

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "cpp_concurrency/hashmaps/lock_based_hashmap.h"

namespace {

struct ConstantHash {
    std::size_t operator()(int) const {
        return 0;
    }
};

struct CountingHash {
    std::atomic<int>* calls = nullptr;

    std::size_t operator()(int key) const {
        calls->fetch_add(1, std::memory_order_relaxed);
        return static_cast<std::size_t>(key);
    }
};

} // namespace

TEST(LockBasedHashmapTest, MissingKeyReturnsDefaultValue) {
    lock_based_hashmap<int, std::string> map(8);

    EXPECT_EQ(map.get_value(42, "missing"), "missing");
}

TEST(LockBasedHashmapTest, AddsAndFindsValue) {
    lock_based_hashmap<int, std::string> map(8);

    map.add_or_update_value(1, "one");
    map.add_or_update_value(2, "two");

    EXPECT_EQ(map.get_value(1, "missing"), "one");
    EXPECT_EQ(map.get_value(2, "missing"), "two");
}

TEST(LockBasedHashmapTest, UpdatesExistingValueWithoutChangingOtherKeys) {
    lock_based_hashmap<int, std::string> map(8);

    map.add_or_update_value(1, "one");
    map.add_or_update_value(2, "two");
    map.add_or_update_value(1, "updated");

    EXPECT_EQ(map.get_value(1, "missing"), "updated");
    EXPECT_EQ(map.get_value(2, "missing"), "two");
}

TEST(LockBasedHashmapTest, RemovesExistingValue) {
    lock_based_hashmap<int, std::string> map(8);

    map.add_or_update_value(7, "seven");
    map.remove_key(7);

    EXPECT_EQ(map.get_value(7, "missing"), "missing");
}

TEST(LockBasedHashmapTest, RemovingMissingValueIsNoOp) {
    lock_based_hashmap<int, std::string> map(8);

    map.add_or_update_value(1, "one");
    map.remove_key(2);

    EXPECT_EQ(map.get_value(1, "missing"), "one");
}

TEST(LockBasedHashmapTest, HandlesHashCollisions) {
    lock_based_hashmap<int, std::string, ConstantHash> map(4);

    map.add_or_update_value(1, "one");
    map.add_or_update_value(2, "two");
    map.add_or_update_value(3, "three");

    EXPECT_EQ(map.get_value(1, "missing"), "one");
    EXPECT_EQ(map.get_value(2, "missing"), "two");
    EXPECT_EQ(map.get_value(3, "missing"), "three");
}

TEST(LockBasedHashmapTest, UsesProvidedHashFunction) {
    std::atomic<int> calls{0};
    lock_based_hashmap<int, std::string, CountingHash> map(8, CountingHash{&calls});

    map.add_or_update_value(1, "one");
    EXPECT_EQ(map.get_value(1, "missing"), "one");

    EXPECT_GE(calls.load(std::memory_order_relaxed), 2);
}

TEST(LockBasedHashmapTest, RejectsZeroBuckets) {
    EXPECT_THROW((lock_based_hashmap<int, int>(0)), std::invalid_argument);
}

TEST(LockBasedHashmapTest, SupportsConcurrentWritesToDistinctKeys) {
    constexpr int thread_count = 8;
    constexpr int keys_per_thread = 1000;
    lock_based_hashmap<int, int> map(31);
    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        threads.emplace_back([&, thread_id]() {
            for (int i = 0; i < keys_per_thread; ++i) {
                const int key = thread_id * keys_per_thread + i;
                map.add_or_update_value(key, key * 2);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int key = 0; key < thread_count * keys_per_thread; ++key) {
        EXPECT_EQ(map.get_value(key, -1), key * 2);
    }
}

TEST(LockBasedHashmapTest, SupportsConcurrentUpdatesToSharedKeys) {
    constexpr int thread_count = 8;
    constexpr int iterations = 1000;
    lock_based_hashmap<int, int, ConstantHash> map(4);
    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        threads.emplace_back([&, thread_id]() {
            for (int i = 0; i < iterations; ++i) {
                map.add_or_update_value(thread_id, i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        EXPECT_EQ(map.get_value(thread_id, -1), iterations - 1);
    }
}
