//
// Created by rahul on 7/3/26.
//

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>
#include "lockfree/spsc_variants.h"

namespace {

struct Payload {
    std::uint64_t sequence;
    std::uint64_t inverse;
    std::array<std::uint64_t, 4> lanes;
};

Payload make_payload(std::uint64_t sequence) {
    return Payload{
        sequence,
        ~sequence,
        {
            sequence ^ 0x9e3779b97f4a7c15ULL,
            sequence * 3 + 1,
            sequence * 5 + 7,
            sequence * 11 + 13,
        },
    };
}

bool payload_is_valid(const Payload& payload) {
    return payload.inverse == ~payload.sequence
           && payload.lanes[0] == (payload.sequence ^ 0x9e3779b97f4a7c15ULL)
           && payload.lanes[1] == payload.sequence * 3 + 1
           && payload.lanes[2] == payload.sequence * 5 + 7
           && payload.lanes[3] == payload.sequence * 11 + 13;
}

void wait_for_start(std::atomic<int>& ready, std::atomic<bool>& start) {
    ready.fetch_add(1, std::memory_order_release);
    while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

template <typename ProducerFunction, typename ConsumerFunction>
void run_started_pair(ProducerFunction producer_body, ConsumerFunction consumer_body) {
    std::atomic<int> ready{0};
    std::atomic<bool> start{false};

    std::thread producer([&]() {
        wait_for_start(ready, start);
        producer_body();
    });

    std::thread consumer([&]() {
        wait_for_start(ready, start);
        consumer_body();
    });

    while (ready.load(std::memory_order_acquire) != 2) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    producer.join();
    consumer.join();
}

template <typename Variant, typename T, std::size_t Capacity>
using QueueFor = typename Variant::template queue<T, Capacity>;

} // namespace

template <typename Variant>
class SPSCQueueTest : public ::testing::Test {};

template <typename VariantList>
struct ToGTestTypes;

template <typename... Variants>
struct ToGTestTypes<spsc_variant_list<Variants...>> {
    using type = ::testing::Types<Variants...>;
};

using SPSCQueueVariants = typename ToGTestTypes<registered_spsc_variants>::type;

TYPED_TEST_SUITE(SPSCQueueTest, SPSCQueueVariants);

TYPED_TEST(SPSCQueueTest, EmptyQueue) {
    QueueFor<TypeParam, int, 8> q;

    int x;
    EXPECT_FALSE(q.pop(x));
}

TYPED_TEST(SPSCQueueTest, PushPop) {
    QueueFor<TypeParam, int, 8> q;

    EXPECT_TRUE(q.push(42));

    int x;
    EXPECT_TRUE(q.pop(x));
    EXPECT_EQ(x, 42);
}

TYPED_TEST(SPSCQueueTest, FIFOOrder) {
    QueueFor<TypeParam, int, 8> q;

    for (int i = 0; i < 5; ++i)
        EXPECT_TRUE(q.push(i));

    for (int i = 0; i < 5; ++i) {
        int x;
        EXPECT_TRUE(q.pop(x));
        EXPECT_EQ(x, i);
    }
}

TYPED_TEST(SPSCQueueTest, EmptyAfterPop) {
    QueueFor<TypeParam, int, 8> q;

    EXPECT_TRUE(q.push(10));

    int x;
    EXPECT_TRUE(q.pop(x));
    EXPECT_EQ(x, 10);

    EXPECT_FALSE(q.pop(x));
}

TYPED_TEST(SPSCQueueTest, FullQueue) {
    QueueFor<TypeParam, int, 8> q;

    // Capacity is effectively Capacity - 1.
    for (int i = 0; i < 7; ++i)
        EXPECT_TRUE(q.push(i));

    EXPECT_FALSE(q.push(100));
}

TYPED_TEST(SPSCQueueTest, RejectsPushWhenFullWithoutOverwriting) {
    QueueFor<TypeParam, int, 8> q;

    for (int i = 0; i < 7; ++i)
        EXPECT_TRUE(q.push(i));

    EXPECT_FALSE(q.push(100));

    for (int i = 0; i < 7; ++i) {
        int x;
        EXPECT_TRUE(q.pop(x));
        EXPECT_EQ(x, i);
    }

    int x;
    EXPECT_FALSE(q.pop(x));
}

TYPED_TEST(SPSCQueueTest, FillEmptyFill) {
    QueueFor<TypeParam, int, 8> q;

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

TYPED_TEST(SPSCQueueTest, WrapAround) {
    QueueFor<TypeParam, int, 8> q;

    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(q.push(i));

        int x;
        EXPECT_TRUE(q.pop(x));
        EXPECT_EQ(x, i);
    }
}

TYPED_TEST(SPSCQueueTest, Interleaved) {
    QueueFor<TypeParam, int, 8> q;

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

TYPED_TEST(SPSCQueueTest, ManyCycles) {
    QueueFor<TypeParam, int, 8> q;

    for (int i = 0; i < 10000; ++i) {
        EXPECT_TRUE(q.push(i));

        int x;
        EXPECT_TRUE(q.pop(x));
        EXPECT_EQ(x, i);
    }
}

TYPED_TEST(SPSCQueueTest, ConcurrentProducerConsumer) {
    constexpr int N = 1'000'000;

    QueueFor<TypeParam, int, 1024> q;

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

TYPED_TEST(SPSCQueueTest, ConcurrentProducerConsumerNoLoss) {
    constexpr int N = 1'000'000;

    QueueFor<TypeParam, int, 1024> q;
    std::atomic<bool> valid{true};
    std::atomic<int> consumed{0};

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

            if (x != i) {
                valid.store(false);
            }

            consumed.fetch_add(1);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(valid.load());
    EXPECT_EQ(consumed.load(), N);
    EXPECT_TRUE(q.empty());
}

TYPED_TEST(SPSCQueueTest, NoLossUnderSmallCapacityContention) {
    constexpr int N = 100'000;

    QueueFor<TypeParam, int, 8> q;
    std::atomic<bool> valid{true};
    std::atomic<int> consumed{0};

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

            if (x != i) {
                valid.store(false);
            }

            consumed.fetch_add(1);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(valid.load());
    EXPECT_EQ(consumed.load(), N);
    EXPECT_TRUE(q.empty());
}

TYPED_TEST(SPSCQueueTest, ConcurrentPayloadIntegrityUnderHeavyContention) {
    constexpr int N = 200'000;

    QueueFor<TypeParam, Payload, 4> q;
    std::atomic<bool> valid{true};
    std::atomic<int> consumed{0};

    run_started_pair(
        [&]() {
            for (int i = 0; i < N; ++i) {
                const Payload payload = make_payload(static_cast<std::uint64_t>(i));

                while (!q.push(payload)) {
                    std::this_thread::yield();
                }

                if ((i % 7) == 0) {
                    std::this_thread::yield();
                }
            }
        },
        [&]() {
            for (int i = 0; i < N; ++i) {
                Payload payload{};

                while (!q.pop(payload)) {
                    std::this_thread::yield();
                }

                const auto expected = static_cast<std::uint64_t>(i);
                if (payload.sequence != expected || !payload_is_valid(payload)) {
                    valid.store(false, std::memory_order_relaxed);
                }

                consumed.fetch_add(1, std::memory_order_relaxed);

                if ((i % 5) == 0) {
                    std::this_thread::yield();
                }
            }
        });

    EXPECT_TRUE(valid.load());
    EXPECT_EQ(consumed.load(), N);
    EXPECT_TRUE(q.empty());
}

TYPED_TEST(SPSCQueueTest, ConcurrentRepeatedSingleSlotHandoffs) {
    constexpr int Rounds = 25;
    constexpr int N = 20'000;

    for (int round = 0; round < Rounds; ++round) {
        QueueFor<TypeParam, int, 2> q;
        std::atomic<bool> valid{true};
        std::atomic<int> consumed{0};

        run_started_pair(
            [&]() {
                for (int i = 0; i < N; ++i) {
                    const int value = round * N + i;

                    while (!q.push(value)) {
                        std::this_thread::yield();
                    }

                    if (((i + round) % 3) == 0) {
                        std::this_thread::yield();
                    }
                }
            },
            [&]() {
                for (int i = 0; i < N; ++i) {
                    int value = 0;

                    while (!q.pop(value)) {
                        std::this_thread::yield();
                    }

                    if (value != round * N + i) {
                        valid.store(false, std::memory_order_relaxed);
                    }

                    consumed.fetch_add(1, std::memory_order_relaxed);

                    if (((i + round) % 4) == 0) {
                        std::this_thread::yield();
                    }
                }
            });

        EXPECT_TRUE(valid.load()) << "round=" << round;
        EXPECT_EQ(consumed.load(), N) << "round=" << round;
        EXPECT_TRUE(q.empty()) << "round=" << round;
    }
}
