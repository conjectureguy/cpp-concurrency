//
// Created by rahul on 7/6/26.
//

#ifndef CPP_CONCURRENCY_RECURSIVE_MUTEX_SPSC_QUEUE_H
#define CPP_CONCURRENCY_RECURSIVE_MUTEX_SPSC_QUEUE_H

#include <array>
#include <cstddef>
#include <mutex>

template <typename T, std::size_t Capacity>
class recursive_mutex_spsc_queue {
public:
    bool push(const T& item);
    bool pop(T& out);
    bool empty() const;
    bool full() const;

private:
    std::array<T, Capacity> queue_;
    int head_{0};
    int tail_{0};
    mutable std::recursive_mutex mutex_;
};

template<typename T, std::size_t Capacity>
bool recursive_mutex_spsc_queue<T, Capacity>::push(const T& item) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (full()) return false;

    auto tail = tail_;
    auto next = (tail + 1) % Capacity;
    queue_[tail] = item;
    tail_ = next;

    return true;
}

template<typename T, std::size_t Capacity>
bool recursive_mutex_spsc_queue<T, Capacity>::pop(T& item) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (empty()) return false;

    auto head = head_;
    auto next = (head + 1) % Capacity;
    item = queue_[head];
    head_ = next;
    return true;
}

template<typename T, std::size_t Capacity>
bool recursive_mutex_spsc_queue<T, Capacity>::empty() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return head_ == tail_;
}

template<typename T, std::size_t Capacity>
bool recursive_mutex_spsc_queue<T, Capacity>::full() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return (tail_ + 1) % Capacity == head_;
}

#endif //CPP_CONCURRENCY_RECURSIVE_MUTEX_SPSC_QUEUE_H
