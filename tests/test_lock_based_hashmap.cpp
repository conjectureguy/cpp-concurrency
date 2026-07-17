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

#include "cpp_concurrency/hashmap_variants.h"

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

template <typename VariantList>
struct ToGTestTypes;

template <typename... Variants>
struct ToGTestTypes<hashmap_variant_list<Variants...>> {
    using type = testing::Types<Variants...>;
};

template <typename Variant>
class LockBasedHashmapTest : public testing::Test {};

using LockBasedHashmapVariants = typename ToGTestTypes<registered_hashmap_variants>::type;

TYPED_TEST_SUITE(LockBasedHashmapTest, LockBasedHashmapVariants);

TYPED_TEST(LockBasedHashmapTest, MissingKeyReturnsDefaultValue) {
    typename TypeParam::template hashmap<int, std::string, std::hash<int>> map(8);

    EXPECT_EQ(map.get_value(42, "missing"), "missing");
}

TYPED_TEST(LockBasedHashmapTest, AddsAndFindsValue) {
    typename TypeParam::template hashmap<int, std::string, std::hash<int>> map(8);

    map.add_or_update_value(1, "one");
    map.add_or_update_value(2, "two");

    EXPECT_EQ(map.get_value(1, "missing"), "one");
    EXPECT_EQ(map.get_value(2, "missing"), "two");
}

TYPED_TEST(LockBasedHashmapTest, UpdatesExistingValueWithoutChangingOtherKeys) {
    typename TypeParam::template hashmap<int, std::string, std::hash<int>> map(8);

    map.add_or_update_value(1, "one");
    map.add_or_update_value(2, "two");
    map.add_or_update_value(1, "updated");

    EXPECT_EQ(map.get_value(1, "missing"), "updated");
    EXPECT_EQ(map.get_value(2, "missing"), "two");
}

TYPED_TEST(LockBasedHashmapTest, RemovesExistingValue) {
    typename TypeParam::template hashmap<int, std::string, std::hash<int>> map(8);

    map.add_or_update_value(7, "seven");
    map.remove_key(7);

    EXPECT_EQ(map.get_value(7, "missing"), "missing");
}

TYPED_TEST(LockBasedHashmapTest, RemovingMissingValueIsNoOp) {
    typename TypeParam::template hashmap<int, std::string, std::hash<int>> map(8);

    map.add_or_update_value(1, "one");
    map.remove_key(2);

    EXPECT_EQ(map.get_value(1, "missing"), "one");
}

TYPED_TEST(LockBasedHashmapTest, HandlesHashCollisions) {
    typename TypeParam::template hashmap<int, std::string, ConstantHash> map(4);

    map.add_or_update_value(1, "one");
    map.add_or_update_value(2, "two");
    map.add_or_update_value(3, "three");

    EXPECT_EQ(map.get_value(1, "missing"), "one");
    EXPECT_EQ(map.get_value(2, "missing"), "two");
    EXPECT_EQ(map.get_value(3, "missing"), "three");
}

TYPED_TEST(LockBasedHashmapTest, UsesProvidedHashFunction) {
    std::atomic<int> calls{0};
    typename TypeParam::template hashmap<int, std::string, CountingHash> map(8, CountingHash{&calls});

    map.add_or_update_value(1, "one");
    EXPECT_EQ(map.get_value(1, "missing"), "one");

    EXPECT_GE(calls.load(std::memory_order_relaxed), 2);
}

TYPED_TEST(LockBasedHashmapTest, RejectsZeroBuckets) {
    using Map = typename TypeParam::template hashmap<int, int, std::hash<int>>;

    EXPECT_THROW((Map(0)), std::invalid_argument);
}

TYPED_TEST(LockBasedHashmapTest, SupportsConcurrentWritesToDistinctKeys) {
    constexpr int thread_count = 8;
    constexpr int keys_per_thread = 1000;
    typename TypeParam::template hashmap<int, int, std::hash<int>> map(31);
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

TYPED_TEST(LockBasedHashmapTest, SupportsConcurrentUpdatesToSharedKeys) {
    constexpr int thread_count = 8;
    constexpr int iterations = 1000;
    typename TypeParam::template hashmap<int, int, ConstantHash> map(4);
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
