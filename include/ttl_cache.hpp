#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

#include "cache.hpp"

namespace ttl {

// 带过期时间（TTL）的缓存：在 LRUCache 之上做「惰性过期」。
//
//   - 每个条目带一个到期时刻；get 命中时若已过期，则当场删除并视为未命中；
//   - 未过期的条目仍由底层 LRU 按容量淘汰；
//   - 注意：过期但不曾被访问的条目会一直占用容量直到被 get 或被 LRU 淘汰
//     （惰性删除的固有代价，可另配定期清理）。
//
// 时钟可注入（默认 std::chrono::steady_clock），便于确定性测试。
template <typename K, typename V>
class TTLCache {
public:
    using key_type   = K;
    using value_type = V;
    using size_type  = std::size_t;
    using clock_fn   = std::function<std::chrono::steady_clock::time_point()>;
    using duration   = std::chrono::steady_clock::duration;

    TTLCache(size_type capacity, duration default_ttl, clock_fn now = default_clock);

    // 用默认 TTL 写入。
    void put(const key_type& key, const value_type& value);
    // 用指定 TTL 写入。
    void put(const key_type& key, const value_type& value, duration ttl);

    // 删除指定 key；返回是否删除成功（key 不存在返回 false）。
    bool erase(const key_type& key);

    // 命中且未过期返回 value；已过期则删除并返回 std::nullopt。
    std::optional<value_type> get(const key_type& key);

    // 查询但不改变访问顺序、也不做惰性删除；已过期视为未命中（const）。
    std::optional<value_type> peek(const key_type& key) const;

    // 是否「存在且未过期」（只读：不刷新访问序、不清理过期条目）。
    bool contains(const key_type& key) const;

    // 当前条目数（可能含已过期但尚未被清理的条目，惰性删除的固有代价）。
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    void clear() noexcept;

private:
    struct Entry {
        value_type                          value;
        std::chrono::steady_clock::time_point expire_at;
    };

    static std::chrono::steady_clock::time_point default_clock() {
        return std::chrono::steady_clock::now();
    }

    lru::LRUCache<key_type, Entry> cache_;
    duration                       default_ttl_;
    clock_fn                       now_;
};

} // namespace ttl

#include "../src/ttl_cache.tpp"
