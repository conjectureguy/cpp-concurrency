//
// Created by rahul on 7/13/26.
//

#ifndef LOCKFREE_REMOVE_FALSE_SHARING_H
#define LOCKFREE_REMOVE_FALSE_SHARING_H

#include <atomic>

template <typename T, std::size_t Capacity>
class alignas(64) remove_false_sharing_spsc_queue {
    static_assert(Capacity > 1, "Capacity must be greater than 1");
    static_assert(std::atomic<int>::is_always_lock_free);

public:
    bool push(const T& item);
    bool pop(T& out);
    bool empty() const;
    bool full() const;

private:
    std::array<T, Capacity> queue_;
    alignas(64) std::atomic<int> head_{0};
    alignas(64) std::atomic<int> tail_{0};
};

template<typename T, std::size_t Capacity>
bool remove_false_sharing_spsc_queue<T, Capacity>::push(const T& item) {
    // check if tail == head else push
    // only producer will write to tail

    // push() owns tail only. So, relaxed is fine but we need acquire for head.
    auto head = head_.load(std::memory_order_acquire);
    auto tail = tail_.load(std::memory_order_relaxed);
    if (head == (tail + 1) % Capacity) {
        return false;
    }

    auto next = (tail + 1) % Capacity;
    queue_[tail] = item;
    tail_.store(next, std::memory_order_release);

    return true;
}

template<typename T, std::size_t Capacity>
bool remove_false_sharing_spsc_queue<T, Capacity>::pop(T& item) {
    auto head = head_.load(std::memory_order_relaxed);
    auto tail = tail_.load(std::memory_order_acquire);
    if (head == tail) {
        return false;
    }

    auto next = (head + 1) % Capacity;
    item = queue_[head];
    head_.store(next, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool remove_false_sharing_spsc_queue<T, Capacity>::empty() const {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
}

template<typename T, std::size_t Capacity>
bool remove_false_sharing_spsc_queue<T, Capacity>::full() const {
    return (tail_.load(std::memory_order_acquire) + 1) % Capacity == head_.load(std::memory_order_acquire);
}

#endif //LOCKFREE_REMOVE_FALSE_SHARING_H
