//
// Created by rahul on 7/3/26.
//

#ifndef LOCKFREE_SPSC_QUEUE_H
#define LOCKFREE_SPSC_QUEUE_H
#include <array>
#include <atomic>

template <typename T, std::size_t Capacity>
class spsc_queue {
    static_assert(Capacity > 1, "Capacity must be greater than 1");
    static_assert(std::atomic<int>::is_always_lock_free);

public:
    bool push(const T& item);
    bool pop(T& out);
    bool empty() const;
    bool full() const;

private:
    std::array<T, Capacity> queue_;
    std::atomic<int> head_{0};
    std::atomic<int> tail_{0};
};

template<typename T, std::size_t Capacity>
bool spsc_queue<T, Capacity>::push(const T& item) {
    // check if tail == head else push
    // only producer will write to tail
    if (full()) return false;

    auto tail = tail_.load();
    auto next = (tail + 1) % Capacity;
    queue_[tail] = item;
    tail_.store(next);

    return true;
}

template<typename T, std::size_t Capacity>
bool spsc_queue<T, Capacity>::pop(T& item) {
    if (empty()) return false;

    auto head = head_.load();
    auto next = (head + 1) % Capacity;
    item = queue_[head];
    head_.store(next);
    return true;
}

template<typename T, std::size_t Capacity>
bool spsc_queue<T, Capacity>::empty() const {
    return head_.load() == tail_.load();
}

template<typename T, std::size_t Capacity>
bool spsc_queue<T, Capacity>::full() const {
    return (tail_.load() + 1) % Capacity == head_.load();
}

#endif //LOCKFREE_SPSC_QUEUE_H
