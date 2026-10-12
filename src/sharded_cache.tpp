// src/sharded_cache.tpp —— ShardedLRUCache 模板实现片段。
//
// 同 cache.tpp：这是「实现片段」，在 include/sharded_cache.hpp 末尾被包含，
// 不要单独编译，也不要重复 #include "sharded_cache.hpp"。

namespace sharded {

template <typename K, typename V>
ShardedLRUCache<K, V>::ShardedLRUCache(size_type total_capacity, size_type shard_count)
    : total_capacity_(total_capacity) {
    if (shard_count == 0) {
        shard_count = 1;
    }
    // 均摊：前 (total % shard_count) 片各多 1 个槽，使各片容量之和精确等于 total_capacity。
    const size_type base = total_capacity / shard_count;
    const size_type rem  = total_capacity % shard_count;
    shards_.reserve(shard_count);
    for (size_type i = 0; i < shard_count; ++i) {
        shards_.push_back(std::make_unique<Shard>(base + (i < rem ? 1 : 0)));
    }
}

template <typename K, typename V>
typename ShardedLRUCache<K, V>::Shard&
ShardedLRUCache<K, V>::shard_for(const key_type& key) {
    return *shards_[hasher_(key) % shards_.size()];
}

template <typename K, typename V>
const typename ShardedLRUCache<K, V>::Shard&
ShardedLRUCache<K, V>::shard_for(const key_type& key) const {
    return *shards_[hasher_(key) % shards_.size()];
}

template <typename K, typename V>
std::optional<typename ShardedLRUCache<K, V>::value_type>
ShardedLRUCache<K, V>::get(const key_type& key) {
    Shard& shard = shard_for(key);
    std::lock_guard<std::mutex> lock(shard.mtx);
    return shard.cache.get(key);
}

template <typename K, typename V>
std::optional<typename ShardedLRUCache<K, V>::value_type>
ShardedLRUCache<K, V>::peek(const key_type& key) const {
    const Shard& shard = shard_for(key);
    std::lock_guard<std::mutex> lock(shard.mtx);
    return shard.cache.peek(key);   // 只读，不刷新 LRU 序
}

template <typename K, typename V>
void ShardedLRUCache<K, V>::put(const key_type& key, const value_type& value) {
    Shard& shard = shard_for(key);
    std::lock_guard<std::mutex> lock(shard.mtx);
    shard.cache.put(key, value);
}

template <typename K, typename V>
bool ShardedLRUCache<K, V>::erase(const key_type& key) {
    Shard& shard = shard_for(key);
    std::lock_guard<std::mutex> lock(shard.mtx);
    return shard.cache.erase(key);
}

template <typename K, typename V>
bool ShardedLRUCache<K, V>::contains(const key_type& key) const {
    const Shard& shard = shard_for(key);
    std::lock_guard<std::mutex> lock(shard.mtx);
    return shard.cache.contains(key);
}

template <typename K, typename V>
typename ShardedLRUCache<K, V>::size_type ShardedLRUCache<K, V>::size() const {
    size_type total = 0;
    for (const auto& shard : shards_) {
        std::lock_guard<std::mutex> lock(shard->mtx);
        total += shard->cache.size();
    }
    return total;
}

template <typename K, typename V>
typename ShardedLRUCache<K, V>::size_type
ShardedLRUCache<K, V>::capacity() const noexcept {
    return total_capacity_;
}

template <typename K, typename V>
typename ShardedLRUCache<K, V>::size_type
ShardedLRUCache<K, V>::shard_count() const noexcept {
    return shards_.size();
}

template <typename K, typename V>
void ShardedLRUCache<K, V>::clear() {
    for (const auto& shard : shards_) {
        std::lock_guard<std::mutex> lock(shard->mtx);
        shard->cache.clear();
    }
}

} // namespace sharded
