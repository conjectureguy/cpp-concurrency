#ifndef LOCKFREE_SPSC_VARIANTS_H
#define LOCKFREE_SPSC_VARIANTS_H

#include <cstddef>
#include <utility>

#include "lockfree/lock_based_spsc_queue.h"
#include "lockfree/spsc_queue.h"

struct lock_free_spsc_variant {
    static constexpr const char* name = "lock_free_spsc";

    template <typename T, std::size_t Capacity>
    using queue = spsc_queue<T, Capacity>;
};

struct lock_based_spsc_variant {
    static constexpr const char* name = "lock_based_spsc";

    template <typename T, std::size_t Capacity>
    using queue = lock_based_spsc_queue<T, Capacity>;
};

template <typename... Variants>
struct spsc_variant_list {};

using registered_spsc_variants = spsc_variant_list<
    lock_free_spsc_variant,
    lock_based_spsc_variant
>;

template <typename Function, typename... Variants>
void for_each_spsc_variant(spsc_variant_list<Variants...>, Function&& function) {
    (std::forward<Function>(function).template operator()<Variants>(), ...);
}

template <typename Function>
void for_each_registered_spsc_variant(Function&& function) {
    for_each_spsc_variant(registered_spsc_variants{}, std::forward<Function>(function));
}

#endif // LOCKFREE_SPSC_VARIANTS_H
