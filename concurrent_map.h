#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };
    std::vector<Bucket> buckets_;

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex&> guard;
        Value& ref_to_value;

        explicit Access(const Key& key, Bucket& bucket) :
            guard(bucket.mutex), ref_to_value(bucket.map[key]) {
        }
        ~Access() = default;
    };

    explicit ConcurrentMap(size_t bucket_count) :
        buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        size_t pos = static_cast<uint64_t>(key) % buckets_.size();
        return Access(key, buckets_[pos]);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        size_t i = 0;
        while (i < buckets_.size()) {
            std::lock_guard<std::mutex> guard(buckets_[i].mutex);
            result.merge(buckets_[i].map);
            ++i;
        }
        return result;
    }

    void erase(const Key& key) {
        size_t pos = static_cast<uint64_t>(key) % buckets_.size();
        std::lock_guard<std::mutex> guard(buckets_[pos].mutex);
        buckets_[pos].map.erase(key);
    }
};