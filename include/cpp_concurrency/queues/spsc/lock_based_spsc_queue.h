//
// Created by rahul on 7/3/26.
//

#ifndef CPP_CONCURRENCY_LOCK_BASED_SPSC_QUEUE_H
#define CPP_CONCURRENCY_LOCK_BASED_SPSC_QUEUE_H
#include <array>
#include <cstddef>
#include <mutex>

// Lock based

template <typename T, std::size_t Capacity>
class lock_based_spsc_queue {
public:
    bool push(const T& item);
    bool pop(T& out);
    bool empty() const;
    bool full() const;

private:
    std::array<T, Capacity> queue_;
    int head_{0};
    int tail_{0};
    mutable std::mutex mutex_;
};

template<typename T, std::size_t Capacity>
bool lock_based_spsc_queue<T, Capacity>::push(const T& item) {
    // check if tail == head else push
    // only producer will write to tail
    std::lock_guard<std::mutex> lock(mutex_);
    if ((tail_ + 1) % Capacity == head_) return false;

    auto tail = tail_;
    auto next = (tail + 1) % Capacity;
    queue_[tail] = item;
    tail_ = next;

    return true;
}

template<typename T, std::size_t Capacity>
bool lock_based_spsc_queue<T, Capacity>::pop(T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (head_ == tail_) return false;

    auto head = head_;
    auto next = (head + 1) % Capacity;
    item = queue_[head];
    head_ = next;
    return true;
}

template<typename T, std::size_t Capacity>
bool lock_based_spsc_queue<T, Capacity>::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return head_ == tail_;
}

template<typename T, std::size_t Capacity>
bool lock_based_spsc_queue<T, Capacity>::full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (tail_ + 1) % Capacity == head_;
}

#endif //CPP_CONCURRENCY_LOCK_BASED_SPSC_QUEUE_H
