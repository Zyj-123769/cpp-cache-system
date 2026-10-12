// src/ttl_cache.tpp —— TTLCache 模板实现片段。
//
// 同 cache.tpp：这是「实现片段」，在 include/ttl_cache.hpp 末尾被包含，
// 不要单独编译，也不要重复 #include "ttl_cache.hpp"。

namespace ttl {

template <typename K, typename V>
TTLCache<K, V>::TTLCache(size_type capacity, duration default_ttl, clock_fn now)
    : cache_(capacity), default_ttl_(default_ttl), now_(std::move(now)) {
    if (!now_) {
        now_ = default_clock;
    }
}

template <typename K, typename V>
void TTLCache<K, V>::put(const key_type& key, const value_type& value) {
    put(key, value, default_ttl_);
}

template <typename K, typename V>
void TTLCache<K, V>::put(const key_type& key, const value_type& value, duration ttl) {
    cache_.put(key, Entry{value, now_() + ttl});
}

template <typename K, typename V>
std::optional<typename TTLCache<K, V>::value_type>
TTLCache<K, V>::get(const key_type& key) {
    auto entry = cache_.get(key);
    if (!entry) {
        return std::nullopt;
    }
    if (entry->expire_at <= now_()) {
        cache_.erase(key);   // 惰性删除：过期即移除
        return std::nullopt;
    }
    return entry->value;
}

template <typename K, typename V>
std::optional<typename TTLCache<K, V>::value_type>
TTLCache<K, V>::peek(const key_type& key) const {
    const auto entry = cache_.peek(key);   // 底层只读查询，不 touch
    if (!entry || entry->expire_at <= now_()) {
        return std::nullopt;               // 已过期视为未命中（不做惰性删除）
    }
    return entry->value;
}

template <typename K, typename V>
bool TTLCache<K, V>::erase(const key_type& key) {
    return cache_.erase(key);   // 转接底层 LRU 的删除
}

template <typename K, typename V>
bool TTLCache<K, V>::contains(const key_type& key) const {
    return peek(key).has_value();   // 只读：不刷新访问序、不清理过期
}

template <typename K, typename V>
typename TTLCache<K, V>::size_type TTLCache<K, V>::size() const noexcept {
    return cache_.size();
}

template <typename K, typename V>
typename TTLCache<K, V>::size_type TTLCache<K, V>::capacity() const noexcept {
    return cache_.capacity();
}

template <typename K, typename V>
void TTLCache<K, V>::clear() noexcept {
    cache_.clear();
}

} // namespace ttl
