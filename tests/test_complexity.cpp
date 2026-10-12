#include <algorithm>
#include <cstddef>
#include <functional>

#include "cache.hpp"
#include "lfu_cache.hpp"
#include "test_common.hpp"

// 计数 key：统计哈希与相等比较次数，用于验证 get/put 的 key-操作数与容量无关。
struct CountingKey {
    int v;
    static inline long hash_calls = 0;
    static inline long eq_calls = 0;
};

inline bool operator==(const CountingKey& a, const CountingKey& b) noexcept {
    ++CountingKey::eq_calls;
    return a.v == b.v;
}

namespace std {
template <>
struct hash<CountingKey> {
    std::size_t operator()(const CountingKey& k) const noexcept {
        ++CountingKey::hash_calls;
        return std::hash<int>{}(k.v);
    }
};
} // namespace std

namespace {

long key_ops() { return CountingKey::hash_calls + CountingKey::eq_calls; }
void reset() { CountingKey::hash_calls = 0; CountingKey::eq_calls = 0; }

// 单次 get/put 的 key 操作数应是常数级，不随容量增长（否则哈希表就退化了）。
// 每次只填 min(capacity, 1000) 个 key，保证被探测的 key 一定在缓存里。
template <typename Cache>
void check_constant_ops(const char* /*name*/) {
    for (std::size_t cap : {std::size_t(10), std::size_t(1000), std::size_t(100000)}) {
        Cache c(cap);
        const int n = static_cast<int>(std::min<std::size_t>(cap, 1000));
        for (int i = 0; i < n; ++i) {
            c.put(CountingKey{i}, i);
        }

        reset();
        CHECK(c.get(CountingKey{0}) == 0);   // 命中，且必在缓存内
        const long get_ops = key_ops();

        c.put(CountingKey{n}, 1);            // 新 key（小容量下会触发淘汰）
        const long put_ops = key_ops() - get_ops;

        CHECK(get_ops <= 8);
        CHECK(put_ops <= 16);
    }
}

} // namespace

int main() {
    check_constant_ops<lru::LRUCache<CountingKey, int>>("LRU");
    check_constant_ops<lfu::LFUCache<CountingKey, int>>("LFU");
    return cachetest::summary();
}
