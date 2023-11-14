#pragma once

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

private:
    struct Bucket {
        std::mutex mux_;

        std::map<Key, Value> map_;
    };
public:
    struct Access {

        Access(const Key& key, Bucket& bucket) : guard(bucket.mux_), ref_to_value(bucket.map_[key]) {};

        std::lock_guard<std::mutex> guard;
        
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : bucket_count_(bucket_count), buckets_(bucket_count) {};

    Access operator[](const Key& key) {
        auto index_bucket = static_cast<uint64_t>(key) % bucket_count_;
        return {key, buckets_[index_bucket]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> resultMap;
        for(auto& [mux, map]: buckets_) {
            std::lock_guard<std::mutex> guard(mux);
            resultMap.insert(map.begin(), map.end());
        }
        return resultMap;
    }

    void erase(const Key& key) {
        auto index_bucket = static_cast<uint64_t>(key) % bucket_count_;
        buckets_[index_bucket].map_.erase(key);
    }

private:
    size_t bucket_count_;

    std::vector<Bucket> buckets_;
};
