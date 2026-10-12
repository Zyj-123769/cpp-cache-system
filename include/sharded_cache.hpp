#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "cache.hpp"

namespace sharded {

// 线程安全的 LRU 缓存：按 key 的哈希把数据分成 N 个分片，每片一个独立 LRU + 独立互斥锁。
//
//   - 锁粒度 = 单分片：不同分片上的操作互不阻塞，分片数越多并发度越高；
//   - total_capacity 均摊到各分片，余数分给前若干片，使实际总容量精确等于 total_capacity；
//     若 total_capacity < shard_count，则有分片容量为 0（落在这些分片上的 key 不会被缓存）；
//   - 未做跨分片一致性（本身也不需要：每个 key 固定落在同一分片）。
template <typename K, typename V>
class ShardedLRUCache {
public:
    using key_type   = K;
    using value_type = V;
    using size_type  = std::size_t;

    explicit ShardedLRUCache(size_type total_capacity, size_type shard_count = 16);

    std::optional<value_type> get(const key_type& key);
    std::optional<value_type> peek(const key_type& key) const;   // 只读，不刷新 LRU 序
    void put(const key_type& key, const value_type& value);
    bool erase(const key_type& key);
    bool contains(const key_type& key) const;   // 是否在缓存中（只读，不刷新 LRU 序）

    size_type size() const;                  // 各分片条目数之和；O(shard_count)（逐片加锁），适合诊断而非高频调用
    size_type capacity() const noexcept;     // 请求的总容量（余数分摊后精确等于构造时的 total_capacity）
    size_type shard_count() const noexcept;
    void clear();

private:
    struct Shard {
        explicit Shard(size_type cap) : cache(cap) {}
        lru::LRUCache<K, V> cache;
        mutable std::mutex  mtx;
    };

    Shard&       shard_for(const key_type& key);
    const Shard& shard_for(const key_type& key) const;

    size_type                            total_capacity_;
    std::vector<std::unique_ptr<Shard>>  shards_;
    std::hash<key_type>                  hasher_;
};

} // namespace sharded

#include "../src/sharded_cache.tpp"
