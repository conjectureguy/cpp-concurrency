#ifndef CPP_CONCURRENCY_HASHMAP_VARIANTS_H
#define CPP_CONCURRENCY_HASHMAP_VARIANTS_H

#include <utility>

#include "cpp_concurrency/hashmaps/lock_based_hashmap.h"
#include "cpp_concurrency/hashmaps/lock_based_hashmap_vector.h"

struct lock_based_hashmap_list_variant {
    static constexpr const char* name = "lock_based_hashmap_list";

    template <typename Key, typename Value, typename Hash>
    using hashmap = lock_based_hashmap<Key, Value, Hash>;
};

struct lock_based_hashmap_vector_variant {
    static constexpr const char* name = "lock_based_hashmap_vector";

    template <typename Key, typename Value, typename Hash>
    using hashmap = lock_based_hashmap_vector<Key, Value, Hash>;
};

template <typename... Variants>
struct hashmap_variant_list {};

using registered_hashmap_variants = hashmap_variant_list<
    lock_based_hashmap_list_variant,
    lock_based_hashmap_vector_variant
>;

template <typename Function, typename... Variants>
void for_each_hashmap_variant(hashmap_variant_list<Variants...>, Function&& function) {
    (std::forward<Function>(function).template operator()<Variants>(), ...);
}

template <typename Function>
void for_each_registered_hashmap_variant(Function&& function) {
    for_each_hashmap_variant(registered_hashmap_variants{}, std::forward<Function>(function));
}

#endif // CPP_CONCURRENCY_HASHMAP_VARIANTS_H
