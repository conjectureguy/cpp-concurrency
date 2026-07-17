//
// Created by rahul on 7/17/26.
//

#ifndef LOCKFREE_LOCK_BASED_HASHMAP_H
#define LOCKFREE_LOCK_BASED_HASHMAP_H
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

template<typename Key, typename Value, typename Hash = std::hash<Key>>
class lock_based_hashmap {
private:
    class bucket_type {
        using bucket_value = std::pair<Key, Value>;
        using bucket = std::list<bucket_value>;
        using bucket_iterator = typename bucket::iterator;
        using const_bucket_iterator = typename bucket::const_iterator;

        bucket bucket_;
        mutable std::shared_mutex mutex_;

        bucket_iterator find_entry_for(const Key& key) {
            for (bucket_iterator it = bucket_.begin(); it != bucket_.end(); ++it) {
                if (it->first == key) return it;
            }
            return bucket_.end();
        }

        const_bucket_iterator find_entry_for(const Key& key) const {
            for (const_bucket_iterator it = bucket_.begin(); it != bucket_.end(); ++it) {
                if (it->first == key) return it;
            }
            return bucket_.end();
        }

    public:
        void add_or_update(const Key& key, const Value& value) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            bucket_iterator it = find_entry_for(key);
            if (it != bucket_.end()) {
                it->second = value;
            }
            else {
                bucket_.emplace_back(key, value);
            }
        }
        Value find_value_for(const Key& key, const Value& default_value) const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            const_bucket_iterator it = find_entry_for(key);
            if (it != bucket_.end()) {
                return it->second;
            }
            return default_value;
        }
        void remove_key(const Key& key) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            bucket_iterator it = find_entry_for(key);
            if (it != bucket_.end()) {
                bucket_.erase(it);
            }
        }
    };
    std::vector<std::unique_ptr<bucket_type>> buckets_;
    Hash hash_function_;

    bucket_type& get_bucket(const Key& key) {
        const std::size_t bucket_id = hash_function_(key) % buckets_.size();
        return *buckets_[bucket_id];
    }

    const bucket_type& get_bucket(const Key& key) const {
        const std::size_t bucket_id = hash_function_(key) % buckets_.size();
        return *buckets_[bucket_id];
    }

public:
    explicit lock_based_hashmap(std::size_t bucket_count = 19, const Hash& hash_function = Hash()):
        buckets_(bucket_count), hash_function_(hash_function) {
        if (bucket_count == 0) {
            throw std::invalid_argument("bucket_count must be greater than zero");
        }

        for (auto& bucket : buckets_) {
            bucket = std::make_unique<bucket_type>();
        }
    }

    lock_based_hashmap(const lock_based_hashmap&) = delete;
    lock_based_hashmap& operator=(const lock_based_hashmap&) = delete;

    Value get_value(const Key& key, const Value& default_value) const {
        return get_bucket(key).find_value_for(key, default_value);
    }

    void add_or_update_value(const Key& key, const Value& value) {
        get_bucket(key).add_or_update(key, value);
    }

    void remove_key(const Key& key) {
        get_bucket(key).remove_key(key);
    }
};

#endif //LOCKFREE_LOCK_BASED_HASHMAP_H
